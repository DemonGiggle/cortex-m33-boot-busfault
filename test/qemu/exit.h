#ifndef QEMU_TEST_EXIT_H
#define QEMU_TEST_EXIT_H

#include <stdint.h>

/* Arm semihosting SYS_EXIT_EXTENDED takes a reason and a host status. */
static inline _Noreturn void QEMU_Exit(uint32_t status)
{
    uint32_t block[] = {0x20026U, status};
    register uint32_t op __asm__("r0") = 0x20U;
    register uint32_t *args __asm__("r1") = block;
    __asm__ volatile ("bkpt 0xab" : "+r"(op), "+r"(args) : : "memory", "cc");
    for (;;) { }
}

#endif
