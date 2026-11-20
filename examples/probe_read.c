#include "boot_busfault.h"

#ifndef DEMO_FAULT_ADDRESS
#error "Set DEMO_FAULT_ADDRESS to an aligned address that faults on your board."
#endif

_Static_assert(((uintptr_t)DEMO_FAULT_ADDRESS & 3U) == 0U,
               "DEMO_FAULT_ADDRESS must be 4-byte aligned.");

enum demo_result {
    DEMO_RUNNING,
    DEMO_VALID_READ_FAILED,
    DEMO_RECOVERY_FAILED,
    DEMO_PASSED,
};

volatile int g_demo_result; /* Shared with the QEMU reporter. */

void Boot_Main(void)
{
    static const uint32_t test_word = 0x12345678U;
    const volatile uint32_t *fault_address =
        (const volatile uint32_t *)(uintptr_t)DEMO_FAULT_ADDRESS;
    uint32_t value = 0;

    g_demo_result = DEMO_RUNNING;
    if (!BusFault_TryRead32(&test_word, &value) || value != test_word) {
        g_demo_result = DEMO_VALID_READ_FAILED;
        return;
    }

    /* The failed read must return false and leave the old value in place. */
    if (BusFault_TryRead32(fault_address, &value) || value != test_word) {
        g_demo_result = DEMO_RECOVERY_FAILED;
        return;
    }

    /* A previous failure must not affect a later successful call. */
    value = 0;
    if (!BusFault_TryRead32(&test_word, &value) || value != test_word) {
        g_demo_result = DEMO_VALID_READ_FAILED;
        return;
    }

    g_demo_result = DEMO_PASSED;
}
