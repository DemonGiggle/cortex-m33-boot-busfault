#include "ARMCM33.h"
#include "boot_busfault.h"
#include "../exit.h"

#define BAD_ADDRESS 0x60000000U
#define PRECISE_FAULT (SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk)

extern const uint8_t Probe_Fault[];
void Probe_Access(uint32_t base, uint32_t value, uint32_t index);
void Probe_OnPSP(uint32_t base, uint32_t value, uint32_t index);
static volatile unsigned completed;
static volatile bool probing;

_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *f)
{
    bool expected = false;
#ifdef EXPECT_FATAL
    expected = probing && f->cfsr == PRECISE_FAULT &&
               f->bfar == BAD_ADDRESS + ADDRESS_OFFSET;
#ifdef ESCALATE
    expected = expected && f->exception == 3 && !f->pc_valid &&
               (f->hfsr & SCB_HFSR_FORCED_Msk);
#else
    expected = expected && f->exception == 5 && f->hfsr == 0 &&
               f->pc_valid && f->pc == (uintptr_t)Probe_Fault;
#endif
#else
    (void)f;
#endif
    QEMU_Exit(expected ? 0 : 1);
}

void Boot_Main(void)
{
#ifdef ESCALATE
    SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTENA_Msk;
    __DSB();
    __ISB();
#endif
    for (unsigned i = 0; i < 2; ++i) {
        g_fault.pc_valid = false;
        probing = true;
#ifdef USE_PSP
        Probe_OnPSP(BAD_ADDRESS, 0x12345678U, 0);
#else
        Probe_Access(BAD_ADDRESS, 0x12345678U, 0);
#endif
        probing = false;
#ifdef EXPECT_FATAL
        QEMU_Exit(1); /* A rejected access must never reach Thread mode. */
#endif
        if (__get_IPSR() != 0 || !g_fault.pc_valid ||
            g_fault.pc != (uintptr_t)Probe_Fault || g_fault.exception != 5 ||
            g_fault.cfsr != PRECISE_FAULT || g_fault.hfsr != 0 ||
            g_fault.bfar != BAD_ADDRESS + ADDRESS_OFFSET || SCB->CFSR != 0) {
            QEMU_Exit(1);
        }
        ++completed;
    }
}

void QEMU_ReportAndExit(void)
{
    QEMU_Exit(completed == 2 ? 0 : 1);
}
