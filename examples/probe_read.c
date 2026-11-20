#include "boot_busfault.h"
#include "ARMCM33.h"

#ifndef DEMO_FAULT_ADDRESS
#error "Set DEMO_FAULT_ADDRESS to an aligned address that faults on your board."
#endif

_Static_assert(((uintptr_t)DEMO_FAULT_ADDRESS & 3U) == 0U,
               "DEMO_FAULT_ADDRESS must be 4-byte aligned.");

extern const uint8_t Demo_ProbeLoad[];
uint32_t Demo_Load32(const volatile uint32_t *address);

enum demo_result {
    DEMO_RUNNING,
    DEMO_VALID_READ_FAILED,
    DEMO_RECOVERY_FAILED,
    DEMO_PASSED,
};

volatile int g_demo_result; /* Shared with the QEMU reporter. */

bool Boot_TryRead32(const volatile uint32_t *address, uint32_t *value)
{
    /* This single-thread test starts a fresh observation for each read. */
    g_fault.pc_valid = false;
    const uint32_t word = Demo_Load32(address);
    if (g_fault.pc_valid && g_fault.pc == (uintptr_t)Demo_ProbeLoad) {
        return false;
    }

    *value = word;
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
    if (Boot_TryRead32(fault_address, &value) || value != test_word ||
        (g_fault.cfsr & SCB_CFSR_BFARVALID_Msk) == 0U ||
        g_fault.bfar != (uintptr_t)fault_address) {
        g_demo_result = DEMO_RECOVERY_FAILED;
        return;
    }

    g_demo_result = DEMO_PASSED;
}
