    .syntax unified
    .cpu cortex-m33
    .thumb

#ifdef OUTSIDE_SECTION
    .text
#else
    .section busfault_skip, "ax", %progbits
#endif
    .balign 2
    .global Probe_Access
    .thumb_func
Probe_Access:
    /* r0 is the base, r1 is the store value, r2 is a zero index. */
#ifdef IN_IT
    cmp r0, r0
    itt eq
#endif
    .global Probe_Fault
Probe_Fault:
    PROBE_OP
    .global Probe_Resume
Probe_Resume:
#ifdef IN_IT
    moveq r2, r2
#endif
    bx lr

#ifdef USE_PSP
    .text
    .global Probe_OnPSP
    .thumb_func
Probe_OnPSP:
    push {r4, lr}
    mrs r4, control
    ldr r3, =probe_stack_top
    msr psp, r3
    orr r3, r4, #2
    msr control, r3
    isb
    bl Probe_Access
    msr control, r4
    isb
    pop {r4, pc}

    .bss
    .balign 8
    .space 512
probe_stack_top:
#endif
