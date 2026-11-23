    .syntax unified
    .cpu cortex-m33
    .thumb

    .equ SCB_SHCSR, 0xE000ED24
    .equ SHCSR_BUSFAULTENA, (1 << 17)

    /* QEMU-only vectors for mps2-an505.
     * Same layout as the sample src/start.S, but Reset_Handler
     * initialises .data/.bss and reports via QEMU_ReportAndExit.
     * Production firmware should keep using src/start.S.
     */
    .section .vectors, "a", %progbits
    .balign 128
    .global __Vectors
__Vectors:
    .word __StackTop        /* 0: Initial MSP */
    .word Reset_Handler     /* 1: Reset */
    .word Default_Handler   /* 2: NMI */
    .word HardFault_Handler /* 3: HardFault */
    .word Default_Handler   /* 4: MemManage */
    .word BusFault_Handler  /* 5: BusFault, offset 0x14 */
    .word Default_Handler   /* 6: UsageFault */
    .word 0, 0, 0, 0        /* 7-10: Reserved without TrustZone */
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
    /* Zero .bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
1:
    cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b
2:
    /* Copy .data */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
3:
    cmp r0, r1
    bhs 4f
    ldr r3, [r2], #4
    str r3, [r0], #4
    b 3b
4:
    /* Enable BusFault once .data and .bss are ready. */
    ldr r0, =SCB_SHCSR
    ldr r1, [r0]
    orr r1, r1, #SHCSR_BUSFAULTENA
    str r1, [r0]
    dsb sy
    isb sy
#ifdef DEMO_USE_PSP
    ldr r0, =demo_stack_top
    msr psp, r0
    movs r0, #2
    msr control, r0
    isb
#endif
    bl Boot_Main
    bl QEMU_ReportAndExit
    b Default_Handler
    .size Reset_Handler, . - Reset_Handler

    .global Default_Handler
    .type Default_Handler, %function
    .thumb_func
Default_Handler:
    b .
    .size Default_Handler, . - Default_Handler

#ifdef DEMO_USE_PSP
    .bss
    .balign 8
    .space 1024
demo_stack_top:
#endif
