    .syntax unified
    .cpu cortex-m33
    .thumb
    .section busfault_skip, "ax", %progbits
    .balign 2
    .global Demo_Load32
    .type Demo_Load32, %function
    .thumb_func
Demo_Load32:
    /* Raw instruction address for the debugger and test. */
    .global Demo_ProbeLoad
Demo_ProbeLoad:
#ifdef DEMO_WIDE_LOAD
    ldr.w   r0, [r0]
#else
    ldr.n   r0, [r0]
#endif
    bx      lr              /* Both encodings resume here. */
    .size Demo_Load32, . - Demo_Load32
