#include <stdint.h>
#include "ARMCM33.h" /* Use your chip's CMSIS header on real hardware. */

extern uint32_t __StackTop; /* Set by the linker script. */
extern void Boot_Main(void); /* The rest of your boot code. */

void Reset_Handler(void);
void HardFault_Handler(void);
void BusFault_Handler(void);
static void Default_Handler(void);

typedef void (*handler_t)(void);

/* Core vectors only. Place this table at the boot vector address. */
__attribute__((used, section(".vectors"), aligned(128)))
static const struct {
    const uint32_t *initial_msp;
    handler_t handlers[15];
} vector_table = {
    .initial_msp = &__StackTop,
    .handlers = {
        Reset_Handler,      /* 1: Reset */
        Default_Handler,    /* 2: NMI */
        HardFault_Handler,  /* 3: HardFault */
        Default_Handler,    /* 4: MemManage */
        BusFault_Handler,   /* 5: BusFault, offset 0x14 */
        Default_Handler,    /* 6: UsageFault */
        0, 0, 0, 0,         /* 7–10: Reserved in this example */
        Default_Handler,    /* 11: SVCall */
        Default_Handler,    /* 12: DebugMonitor */
        0,                  /* 13: Reserved */
        Default_Handler,    /* 14: PendSV */
        Default_Handler,    /* 15: SysTick */
    },
};

/* Read this record with a debugger after a fault. */
volatile struct {
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
} g_fault;

void Reset_Handler(void)
{
    /* Enable BusFault before the rest of boot runs. */
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
    __DSB();
    __ISB();

    Boot_Main();
    Default_Handler();
}

static void Save_Fault_And_Stop(void)
{
    const uint32_t cfsr = SCB->CFSR;

    g_fault.cfsr = cfsr;
    g_fault.hfsr = SCB->HFSR;

    /* Read BFAR only if the CPU marked its address as valid. */
    g_fault.bfar = (cfsr & SCB_CFSR_BFARVALID_Msk) ? SCB->BFAR : 0U;

    Default_Handler();
}

void BusFault_Handler(void)
{
    Save_Fault_And_Stop();
}

void HardFault_Handler(void)
{
    /* A BusFault can reach this handler if it cannot run on its own. */
    Save_Fault_And_Stop();
}

static void Default_Handler(void)
{
    /* Wait here for a debugger. */
    for (;;) {
        __NOP();
    }
}
