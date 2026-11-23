    .syntax unified
    .cpu cortex-m33
    .thumb

#ifndef BUSFAULT_USE_VENDOR_STARTUP
    .equ SCB_SHCSR, 0xE000ED24
    .equ SHCSR_BUSFAULTENA, (1 << 17)

    /* Arm M33 Guide 2.3.4: vector order and exception offsets. */
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
    /* Arm M33 Guide 4.2.10: SHCSR bit 17 enables BusFault. */
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
#endif
