#include "../boot_busfault.h"
#include "ARMCM33.h"

#ifndef DEMO_FAULT_ADDRESS
#error "Set DEMO_FAULT_ADDRESS to an aligned address that faults on your board."
#endif

_Static_assert(((uintptr_t)DEMO_FAULT_ADDRESS & 3U) == 0U,
               "DEMO_FAULT_ADDRESS must be 4-byte aligned.");

extern const uint8_t Demo_ProbeLoad[];
extern const uint8_t Demo_ProbeResume[];
bool Boot_TryRead32(const volatile uint32_t *address, uint32_t *value);

enum demo_result {
    DEMO_RUNNING,
    DEMO_VALID_READ_FAILED,
    DEMO_RECOVERY_FAILED,
    DEMO_PASSED,
};

volatile enum demo_result g_demo_result;

bool BusFault_GetFixup(const volatile BusFaultRecord *fault, BusFaultFixup *fixup)
{
    /* Recover only from the test load at the chosen data address. */
    if (fault->pc != (uintptr_t)Demo_ProbeLoad ||
        (fault->cfsr & SCB_CFSR_BFARVALID_Msk) == 0U ||
        fault->bfar != (uintptr_t)DEMO_FAULT_ADDRESS) {
        return false;
    }

    fixup->resume_pc = (uintptr_t)Demo_ProbeResume;
    fixup->resume_r0 = 0;
    return true;
}

void Boot_Main(void)
{
    static const uint32_t test_word = 0x12345678U;
    const volatile uint32_t *fault_address =
        (const volatile uint32_t *)(uintptr_t)DEMO_FAULT_ADDRESS;
    uint32_t value = 0;

    g_demo_result = DEMO_RUNNING;
    if (!Boot_TryRead32(&test_word, &value) || value != test_word) {
        g_demo_result = DEMO_VALID_READ_FAILED;
        return;
    }

    /* The failed read must return false and leave the old value in place. */
    if (Boot_TryRead32(fault_address, &value) || value != test_word) {
        g_demo_result = DEMO_RECOVERY_FAILED;
        return;
    }

    g_demo_result = DEMO_PASSED;
}
