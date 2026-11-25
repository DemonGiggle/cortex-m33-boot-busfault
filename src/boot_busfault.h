#ifndef BOOT_BUSFAULT_H
#define BOOT_BUSFAULT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t exception;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar; /* Valid when CFSR.BFARVALID is set. */
    uintptr_t pc;  /* Valid when pc_valid is true. */
    bool pc_valid;
} BusFaultRecord;

extern volatile BusFaultRecord g_fault;

/* Thread mode, aligned source, valid writable output. On a precise BusFault
 * at the source load, return false and leave *out unchanged. */
bool BusFault_TryRead32(const volatile uint32_t *address, uint32_t *out)
    __attribute__((warn_unused_result));

/* By default, mask interrupts, print via BusFault_FatalWrite if provided,
 * then halt. A board override should put the board in a safe state. */
_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *fault);

/* Optional sink for the default hook. Consume each NUL-terminated fragment
 * before returning. Use bounded polling and avoid locks or interrupts. */
void BusFault_FatalWrite(const char *text);

#endif
