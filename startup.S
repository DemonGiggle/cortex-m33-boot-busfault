    .syntax unified
    .cpu cortex-m33
    .thumb

    .equ SCB_SHCSR, 0xE000ED24
    .equ SHCSR_BUSFAULTENA, (1 << 17)

    /* Core vectors. The linker places this table at the boot address. */
    .section .vectors, "a", %progbits
    .balign 128
    .global __Vectors
__Vectors:
    .word __StackTop        /* 0: Initial MSP */
    .word Reset_Handler     /* 1: Reset */
    .word Default_Handler   /* 2: NMI */
    .word HardFault_Handler /* 3: HardFault */
    .word Default_Handler   /* 4: MemManage */
    .word BusFault_Handler /* 5: BusFault, offset 0x14 */
    .word Default_Handler   /* 6: UsageFault */
    .word 0, 0, 0, 0        /* 7–10: Reserved without TrustZone */
    .word Default_Handler   /* 11: SVCall */
    .word Default_Handler   /* 12: DebugMonitor */
    .word 0                 /* 13: Reserved */
    .word Default_Handler   /* 14: PendSV */
    .word Default_Handler   /* 15: SysTick */

    .text
    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    /* Set SHCSR.BUSFAULTENA before the rest of boot runs. */
    ldr     r0, =SCB_SHCSR
    ldr     r1, [r0]
    orr     r1, r1, #SHCSR_BUSFAULTENA
    str     r1, [r0]
    dsb     sy
    isb     sy

    bl      Boot_Main
    b       Default_Handler
    .size Reset_Handler, . - Reset_Handler

    .global Default_Handler
    .type Default_Handler, %function
    .thumb_func
Default_Handler:
    b       .
    .size Default_Handler, . - Default_Handler

    /* r0 holds the address. r1 points to the output word. */
    .global Boot_TryRead32
    .type Boot_TryRead32, %function
    .thumb_func
Boot_TryRead32:
    movs    r3, #1          /* Assume the load will succeed. */
    .global Boot_ProbeLoad
Boot_ProbeLoad:
    ldr.n   r2, [r0]        /* This 16-bit load can be skipped. */
    .global Boot_ProbeResume
Boot_ProbeResume:
    cbz     r3, 1f          /* The handler sets r3 to zero on a fault. */
    str     r2, [r1]        /* Save the word only after a good read. */
1:
    mov     r0, r3          /* Return true for success, false for a fault. */
    bx      lr
    .size Boot_TryRead32, . - Boot_TryRead32
