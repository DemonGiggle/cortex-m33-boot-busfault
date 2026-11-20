#include "ARMCM33.h"
#include "boot_busfault.h"
#include "../exit.h"

#define BAD_ADDRESS 0x60000000U
#define PRECISE_FAULT (SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk)

extern const uint8_t Probe_Fault[], BusFault_Read32_Load[];
void Probe_Access(uint32_t base, uint32_t value, uint32_t index);
void Probe_OnPSP(uint32_t base, uint32_t value, uint32_t index);
static volatile bool probing;

_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *f)
{
    bool expected = probing && f->cfsr == PRECISE_FAULT &&
                    f->bfar == BAD_ADDRESS + ADDRESS_OFFSET;
#ifdef ESCALATE
    expected = expected && f->exception == 3 && !f->pc_valid &&
               (f->hfsr & SCB_HFSR_FORCED_Msk);
#else
    expected = expected && f->exception == 5 && f->hfsr == 0 && f->pc_valid;
#ifdef BAD_OUTPUT
    expected = expected && f->pc != (uintptr_t)BusFault_Read32_Load;
#else
    expected = expected && f->pc == (uintptr_t)Probe_Fault;
#endif
#endif
    QEMU_Exit(expected ? 0 : 1);
}

void Boot_Main(void)
{
    probing = true;
#if defined(ESCALATE)
    uint32_t value;
    SCB->SHCSR &= ~SCB_SHCSR_BUSFAULTENA_Msk;
    __DSB();
    __ISB();
    if (BusFault_TryRead32((const volatile uint32_t *)BAD_ADDRESS, &value))
        QEMU_Exit(1);
#elif defined(BAD_OUTPUT)
    static const uint32_t readable = 123;
    if (BusFault_TryRead32(&readable, (uint32_t *)BAD_ADDRESS))
        QEMU_Exit(1);
#elif defined(USE_PSP)
    Probe_OnPSP(BAD_ADDRESS, 0x12345678U, 0);
#else
    Probe_Access(BAD_ADDRESS, 0x12345678U, 0);
#endif
    QEMU_Exit(1); /* Only the expected fatal hook may pass this test. */
}

void QEMU_ReportAndExit(void)
{
    QEMU_Exit(1);
}
