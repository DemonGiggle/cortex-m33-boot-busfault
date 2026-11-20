    .syntax unified
    .cpu cortex-m33
    .thumb

    /* Caller-opted-in 32-bit probe. Only the load at BusFault_Read32_Load
     * is guarded: a precise BusFault there resumes at BusFault_Read32_Failed
     * with false and leaves *out untouched. The store, the success return,
     * and every other instruction in the image are fatal on fault. */
    .section .text.BusFault_TryRead32, "ax", %progbits
    .balign 2

    /* bool BusFault_TryRead32(const volatile uint32_t *address, uint32_t *out) */
    .global BusFault_TryRead32
    .type BusFault_TryRead32, %function
    .thumb_func
BusFault_TryRead32:
    /* Raw PC labels, not Thumb function pointers. Only the load is guarded. */
    .global BusFault_Read32_Load
BusFault_Read32_Load:
    ldr     r2, [r0]
    str     r2, [r1]
    movs    r0, #1
    bx      lr

    .global BusFault_Read32_Failed
BusFault_Read32_Failed:
    movs    r0, #0
    bx      lr
    .size BusFault_TryRead32, . - BusFault_TryRead32
