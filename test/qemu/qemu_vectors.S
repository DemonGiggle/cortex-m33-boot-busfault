    .syntax unified
    .cpu cortex-m33
    .thumb

    /* QEMU-only vectors for mps2-an505.
     * Same layout as the sample startup.S, but Reset_Handler
     * initialises .data/.bss and reports via QEMU_ReportAndExit.
     * Production firmware should keep using startup.S.
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
    bl BusFault_Enable
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
