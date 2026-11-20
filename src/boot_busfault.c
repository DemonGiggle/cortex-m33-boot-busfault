#include "boot_busfault.h"
#include <stddef.h>

/* Arm M33 Guide 4.2: System Control Block registers and CFSR bits. */
#define BUSFAULT_SCB_REG(addr)      (*(volatile uint32_t *)(uintptr_t)(addr))
#define BUSFAULT_SCB_CFSR           BUSFAULT_SCB_REG(0xE000ED28UL)
#define BUSFAULT_SCB_HFSR           BUSFAULT_SCB_REG(0xE000ED2CUL)
#define BUSFAULT_SCB_BFAR           BUSFAULT_SCB_REG(0xE000ED38UL)
#define BUSFAULT_CFSR_PRECISERR_MSK (1UL << 9)
#define BUSFAULT_CFSR_BFARVALID_MSK (1UL << 15)

#define EXC_RETURN_THREAD      (1UL << 3)
#define EXC_RETURN_BASIC_FRAME (1UL << 4)
#define XPSR_THUMB             (1UL << 24)
#define XPSR_IT_ICI            ((3UL << 25) | (0x3FUL << 10))

/* Arm M33 Guide 2.3.7.1, Fig. 2-6: basic exception frame. */
struct exception_frame {
    uint32_t r0, r1, r2, r3;
    uint32_t r12, lr, pc, xpsr;
};
_Static_assert(offsetof(struct exception_frame, pc) == 0x18, "saved PC offset");

/* Optional linker bounds for readable code that can tolerate a skipped access. */
extern const uint8_t __start_busfault_skip[] __attribute__((weak));
extern const uint8_t __stop_busfault_skip[] __attribute__((weak));

volatile BusFaultRecord g_fault;

__attribute__((weak))
bool BusFault_GetFixup(const volatile BusFaultRecord *fault, BusFaultFixup *fixup)
{
    const uintptr_t pc = fault->pc;
    const uintptr_t start = (uintptr_t)__start_busfault_skip;
    const uintptr_t end = (uintptr_t)__stop_busfault_skip;

    if (!fault->pc_valid || (pc & 1U) != 0U ||
        pc < start || pc >= end ||
        end - pc < 2U) {
        return false;
    }

    /* Arm DDI 0553B.n, B6.3 and C2.1: bits 15:11 are in byte 1.
     * Prefixes 11101, 11110 and 11111 start a 32-bit T32 instruction. */
    const uint8_t prefix = ((const volatile uint8_t *)pc)[1] >> 3;
    const uintptr_t size = (prefix >= 0x1DU) ? 4U : 2U;
    /* Keep the first halfword of the next instruction inside the section. */
    if (end - pc < size + 2U) {
        return false;
    }

    fixup->resume_pc = pc + size;
    return true;
}

__attribute__((weak))
_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *fault)
{
    (void)fault;
    __asm__ volatile ("cpsid i" ::: "memory");
    for (;;) {
        __asm__ volatile ("nop");
    }
}

static uint32_t Capture_Fault(void)
{
    const uint32_t cfsr = BUSFAULT_SCB_CFSR;

    uint32_t exception;
    __asm__ volatile ("mrs %0, ipsr" : "=r" (exception));
    g_fault.exception = exception;
    g_fault.cfsr = cfsr;
    g_fault.hfsr = BUSFAULT_SCB_HFSR;
    /* Arm M33 Guide 4.2.11.2 and 4.2.14: BFAR holds the data address. */
    g_fault.bfar = (cfsr & BUSFAULT_CFSR_BFARVALID_MSK) ? BUSFAULT_SCB_BFAR : 0U;
    g_fault.pc = 0;
    g_fault.pc_valid = false;
    return cfsr;
}

__attribute__((used))
static void BusFault_Handle(uint32_t exc_return,
                            volatile struct exception_frame *frame)
{
    const uint32_t cfsr = Capture_Fault();
    const uint32_t basic_thread = EXC_RETURN_THREAD | EXC_RETURN_BASIC_FRAME;

    /* Arm M33 Guide 4.2.11.2: PRECISERR identifies the saved instruction.
     * STKERR can leave an incomplete frame. Check status before reading it. */
    if ((cfsr & ~BUSFAULT_CFSR_BFARVALID_MSK) != BUSFAULT_CFSR_PRECISERR_MSK ||
        (exc_return & basic_thread) != basic_thread ||
        frame == 0 || ((uintptr_t)frame & 3U) != 0U) {
        BusFault_OnFatal(&g_fault);
    }

    g_fault.pc = frame->pc;
    g_fault.pc_valid = true;

    /* Arm M33 Guide 2.1.3.6.3: IT/ICI bits carry execution state. */
    if ((frame->xpsr & (XPSR_THUMB | XPSR_IT_ICI)) != XPSR_THUMB) {
        BusFault_OnFatal(&g_fault);
    }

    /* A skipped instruction supplies no new result. Keep the saved r0. */
    BusFaultFixup fixup = { .resume_r0 = frame->r0 };
    if (!BusFault_GetFixup(&g_fault, &fixup) ||
        fixup.resume_pc == 0U || (fixup.resume_pc & 1U) != 0U) {
        BusFault_OnFatal(&g_fault);
    }

    frame->r0 = fixup.resume_r0;
    frame->pc = fixup.resume_pc;
    BUSFAULT_SCB_CFSR = cfsr; /* Arm M33 Guide 4.2.11: write one to clear. */
    __asm__ volatile ("dsb sy" ::: "memory");
}

__attribute__((naked))
void BusFault_Handler(void)
{
    /* Arm M33 Guide 2.3.7.2: EXC_RETURN bit 2 selects MSP or PSP.
     * Branch with LR intact so exception return restores the saved PC. */
    __asm volatile (
        "mov r0, lr\n"
        "tst r0, #4\n"
        "ite eq\n"
        "mrseq r1, msp\n"
        "mrsne r1, psp\n"
        "b BusFault_Handle\n"
    );
}

void HardFault_Handler(void)
{
    Capture_Fault();
    BusFault_OnFatal(&g_fault);
}
