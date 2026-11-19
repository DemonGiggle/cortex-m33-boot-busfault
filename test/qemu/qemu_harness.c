#include <stdint.h>
#include <stdbool.h>
#include "boot_busfault.h"
#include "ARMCM33.h"
#include "exit.h"

/* QEMU-only test harness: UART0 report, then semihosting exit.
 * Production files (boot_busfault.c, examples/probe_read.*) are unchanged.
 */

extern volatile int g_demo_result;
extern const uint8_t Demo_ProbeLoad[];

#define UART0_BASE 0x50200000UL /* secure alias of 0x40200000 */
#define UART_DATA  (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_STATE (*(volatile uint32_t *)(UART0_BASE + 0x04))
#define UART_CTRL  (*(volatile uint32_t *)(UART0_BASE + 0x08))
#define UART_STATE_TXFULL (1U << 0)

static void uart_init(void)
{
    UART_CTRL = (1U << 0) | (1U << 1); /* TX_EN | RX_EN */
}

static void uart_putc(char c)
{
    int timeout = 1000000;
    while ((UART_STATE & UART_STATE_TXFULL) && --timeout) { }
    UART_DATA = (uint32_t)c;
}

static void out_str(const char *s)
{
    /* UART only. Semihosting is used for exit below, so text stays single-copy. */
    for (const char *p = s; *p; p++) {
        if (*p == '\n') uart_putc('\r');
        uart_putc(*p);
    }
}

static void out_hex32(uint32_t v)
{
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        uint32_t n = (v >> (28 - i*4)) & 0xF;
        buf[2+i] = (char)(n < 10 ? '0'+n : 'a'+n-10);
    }
    buf[10] = '\0';
    out_str(buf);
}

static void out_dec(int v)
{
    char buf[12];
    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    int i = 11;
    buf[i--] = '\0';
    if (v == 0) buf[i--] = '0';
    while (v > 0 && i >= 0) { buf[i--] = (char)('0' + v%10); v/=10; }
    if (neg && i >= 0) buf[i--] = '-';
    out_str(&buf[i+1]);
}

void QEMU_ReportAndExit(void)
{
    uart_init();

    __DSB();

    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;
    uint32_t shcsr = SCB->SHCSR;

    out_str("\n=== cortex-m33-boot-busfault QEMU ===\n");
    out_str("g_demo_result="); out_dec((int)g_demo_result); out_str(" (0=RUNNING 1=VALID_FAIL 2=RECOVERY_FAIL 3=PASSED)\n");
    out_str("g_fault.exception="); out_dec((int)g_fault.exception); out_str("\n");
    out_str("g_fault.cfsr="); out_hex32(g_fault.cfsr); out_str("\n");
    out_str("g_fault.hfsr="); out_hex32(g_fault.hfsr); out_str("\n");
    out_str("g_fault.bfar="); out_hex32(g_fault.bfar); out_str("\n");
    out_str("g_fault.pc="); out_hex32((uint32_t)g_fault.pc); out_str(" valid="); out_dec(g_fault.pc_valid ? 1 : 0); out_str("\n");
    out_str("live SCB->CFSR="); out_hex32(cfsr); out_str(" HFSR="); out_hex32(hfsr);
    out_str(" BFAR="); out_hex32(bfar); out_str(" SHCSR="); out_hex32(shcsr); out_str("\n");
    out_str("Demo_ProbeLoad="); out_hex32((uint32_t)(uintptr_t)Demo_ProbeLoad); out_str("\n");

    if ((int)g_demo_result == 3 && g_fault.exception == 5 &&
        g_fault.cfsr == (SCB_CFSR_PRECISERR_Msk | SCB_CFSR_BFARVALID_Msk) &&
        cfsr == 0 && hfsr == 0) {
        out_str("RESULT: PASS\n");
        QEMU_Exit(0);
    } else {
        out_str("RESULT: FAIL\n");
        QEMU_Exit(1);
    }
}
