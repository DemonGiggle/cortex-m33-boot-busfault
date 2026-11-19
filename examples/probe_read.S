    .syntax unified
    .cpu cortex-m33
    .thumb
    .text

    .global Boot_TryRead32
    .type Boot_TryRead32, %function
    .thumb_func
Boot_TryRead32:
    mov     r2, r0          /* Keep the address in r2. */
    movs    r0, #1          /* Return true unless the handler sets r0 to zero. */
    .global Demo_ProbeLoad
Demo_ProbeLoad:
    ldr.n   r3, [r2]        /* This 16-bit load can be skipped. */
    .global Demo_ProbeResume
Demo_ProbeResume:
    cbz     r0, 1f          /* Skip the output store after a failed read. */
    str     r3, [r1]
1:
    bx      lr
    .size Boot_TryRead32, . - Boot_TryRead32
