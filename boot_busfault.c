#include "boot_busfault.h"
#include "ARMCM33.h" /* Use your chip's CMSIS header. */

#define EXC_RETURN_THREAD      (1UL << 3)
#define EXC_RETURN_BASIC_FRAME (1UL << 4)
#define XPSR_THUMB             (1UL << 24)
#define XPSR_IT_ICI            ((3UL << 25) | (0x3FUL << 10))

/* The CPU saves these registers in this order. */
struct exception_frame {
    uint32_t r0, r1, r2, r3;
    uint32_t r12, lr, pc, xpsr;
};

volatile BusFaultRecord g_fault;

__attribute__((weak))
bool BusFault_GetFixup(const volatile BusFaultRecord *fault, BusFaultFixup *fixup)
{
    (void)fault;
    (void)fixup;
    return false;
}

__attribute__((weak))
_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *fault)
{
    (void)fault;
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

static uint32_t Capture_Fault(void)
{
    const uint32_t cfsr = SCB->CFSR;

    g_fault.exception = __get_IPSR();
    g_fault.cfsr = cfsr;
    g_fault.hfsr = SCB->HFSR;
    g_fault.bfar = (cfsr & SCB_CFSR_BFARVALID_Msk) ? SCB->BFAR : 0U;
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

    /* A stacking fault can leave an incomplete frame. */
    if ((cfsr & ~SCB_CFSR_BFARVALID_Msk) != SCB_CFSR_PRECISERR_Msk ||
        (exc_return & basic_thread) != basic_thread ||
        frame == 0 || ((uintptr_t)frame & 3U) != 0U) {
        BusFault_OnFatal(&g_fault);
    }

    g_fault.pc = frame->pc;
    g_fault.pc_valid = true;

    /* Resume only from plain Thumb state, outside IT or ICI state. */
    if ((frame->xpsr & (XPSR_THUMB | XPSR_IT_ICI)) != XPSR_THUMB) {
        BusFault_OnFatal(&g_fault);
    }

    BusFaultFixup fixup = {0};
    if (!BusFault_GetFixup(&g_fault, &fixup) ||
        fixup.resume_pc == 0U || (fixup.resume_pc & 1U) != 0U) {
        BusFault_OnFatal(&g_fault);
    }

    frame->r0 = fixup.resume_r0;
    frame->pc = fixup.resume_pc;
    SCB->CFSR = cfsr; /* Clear only the flags saved above. */
    __DSB();
}

__attribute__((naked))
void BusFault_Handler(void)
{
    /* EXC_RETURN bit 2 selects MSP or PSP. Keep LR for exception return. */
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
