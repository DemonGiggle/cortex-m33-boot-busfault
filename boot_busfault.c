#include <stdint.h>
#include "ARMCM33.h" /* Use your chip's CMSIS header on real hardware. */

#define EXC_RETURN_THREAD (1UL << 3)
#define EXC_RETURN_BASIC_FRAME (1UL << 4)

extern const uint8_t Boot_ProbeLoad[];
extern const uint8_t Boot_ProbeResume[];
_Noreturn void Default_Handler(void);

/* The CPU saves these registers in this order. */
struct exception_frame {
    uint32_t r0, r1, r2, r3;
    uint32_t r12, lr, pc, xpsr;
};

/* Read this record with a debugger after a fault. */
volatile struct {
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
} g_fault;

static uint32_t Save_Fault(void)
{
    const uint32_t cfsr = SCB->CFSR;

    g_fault.cfsr = cfsr;
    g_fault.hfsr = SCB->HFSR;
    g_fault.bfar = (cfsr & SCB_CFSR_BFARVALID_Msk) ? SCB->BFAR : 0U;

    return cfsr;
}

__attribute__((used))
static void BusFault_Handle(uint32_t exc_return,
                            volatile struct exception_frame *frame)
{
    const uint32_t cfsr = Save_Fault();
    const uint32_t basic_thread = EXC_RETURN_THREAD | EXC_RETURN_BASIC_FRAME;

    /* Check for a precise load fault before reading the saved PC. */
    if ((cfsr & ~SCB_CFSR_BFARVALID_Msk) != SCB_CFSR_PRECISERR_Msk ||
        (exc_return & basic_thread) != basic_thread ||
        frame->pc != (uintptr_t)Boot_ProbeLoad) {
        Default_Handler();
    }

    /* Report a failed read and resume at the instruction after the LDR. */
    frame->r3 = 0;
    frame->pc = (uintptr_t)Boot_ProbeResume;

    SCB->CFSR = cfsr; /* Clear the saved flags by writing ones. */
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
    /* Save the fault registers if the BusFault escalated to HardFault. */
    Save_Fault();
    Default_Handler();
}
