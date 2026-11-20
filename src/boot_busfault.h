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

typedef struct {
    uintptr_t resume_pc; /* Even address of a known Thumb instruction. */
    uint32_t resume_r0;  /* Starts with the saved r0. A board hook may change it. */
} BusFaultFixup;

extern volatile BusFaultRecord g_fault;

void BusFault_Enable(void);

/* Default policy: skip one instruction in the busfault_skip section. */
bool BusFault_GetFixup(const volatile BusFaultRecord *fault, BusFaultFixup *fixup);

/* Put the board into a safe state and halt or reset. */
_Noreturn void BusFault_OnFatal(const volatile BusFaultRecord *fault);

#endif
