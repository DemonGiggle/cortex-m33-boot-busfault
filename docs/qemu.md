# Testing on QEMU (mps2-an505, Cortex-M33)

This runs the in-repo sample (`boot_busfault.c` + `examples/probe_read.*`)
on QEMU with no board needed. It checks both Thumb encodings:

- narrow: `ldr.n r0, [r0]` (`0x6800`)
- wide: `ldr.w r0, [r0]` (`0xf8d0 0x0000`)

Both must print `RESULT: PASS`.

Production files are untouched. QEMU-only support files live under
`test/qemu/` (vectors, linker script, UART + semihosting harness, CMSIS subset).

Tested with:

- `arm-none-eabi-gcc 14.2.1` (`14:14.2.rel1-1` on Debian)
- `QEMU emulator version 10.0.13`
- `gdb-multiarch 16.3` (optional, for the GDB check)

Install on Debian/Ubuntu:

```sh
sudo apt update
sudo apt install gcc-arm-none-eabi qemu-system-arm gdb-multiarch
```

## Layout

```text
boot_busfault.c / boot_busfault.h   production handler (unchanged)
startup.S                           production vectors + BusFault_Enable (unchanged)
examples/probe_read.c               Boot_Main + Boot_TryRead32 (unchanged)
examples/probe_read.S               Demo_Load32 in busfault_skip (unchanged)
test/qemu/qemu_vectors.S            QEMU-only Reset (init .data/.bss, call report)
test/qemu/qemu_harness.c            QEMU-only UART report + semihosting exit
test/qemu/link.ld                   AN505 memory map (FLASH 0x10000000, RAM 0x38000000)
test/qemu/cmsis/                    vendored CMSIS headers matching ARMCM33 no-DSP/no-FPU
docs/qemu.md                        this file
```

Why these addresses:

- AN505 boots Secure with `SVTOR = 0x10000000`. That is the Secure alias of
  `0x0`, so vectors + code must link at `0x10000000`, not `0x0`.
  Otherwise the core fetches garbage vectors and locks up before `Reset`.
- `0x38000000` is the Secure alias of AN505 SSRAM `0x28000000`.
  Data/stack stay Secure and MPC-clean.
- Fault address `0x60000000` is unmapped on AN505, 4-byte aligned, so the
  `ldr` raises a precise BusFault. UART0 is `0x40200000`,
  Secure alias `0x50200000`.

## 0. Clone and enter the repo

```sh
git clone https://github.com/DemonGiggle/cortex-m33-boot-busfault
cd cortex-m33-boot-busfault
mkdir -p build/qemu
```

All commands below run from the repo root.

## 1. Build the narrow (16-bit) test

`+nodsp+nofp` matches the vendored `ARMCM33.h` (no DSP, no FPU).
`startup.S` is built with `-DBUSFAULT_USE_VENDOR_STARTUP` so only
`BusFault_Enable` is used; vectors/reset come from `qemu_vectors.S`.

```sh
arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -I test/qemu/cmsis -I . -DDEMO_FAULT_ADDRESS=0x60000000U \
  -c boot_busfault.c -o build/qemu/boot.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -I test/qemu/cmsis -I . -DDEMO_FAULT_ADDRESS=0x60000000U \
  -c examples/probe_read.c -o build/qemu/probe.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -I test/qemu/cmsis -I . \
  -c test/qemu/qemu_harness.c -o build/qemu/harness.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -c test/qemu/qemu_vectors.S -o build/qemu/vectors.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -c examples/probe_read.S -o build/qemu/probe_asm.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -DBUSFAULT_USE_VENDOR_STARTUP \
  -c startup.S -o build/qemu/startup_enable.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -nostartfiles -T test/qemu/link.ld \
  build/qemu/vectors.o build/qemu/boot.o build/qemu/probe.o \
  build/qemu/probe_asm.o build/qemu/harness.o build/qemu/startup_enable.o \
  -o build/qemu/test_narrow.elf -Wl,-Map=build/qemu/test_narrow.map
```

Confirm the narrow encoding:

```sh
arm-none-eabi-objdump -d build/qemu/test_narrow.elf | grep -A2 "<Demo_Load32>:"
```

Expected:

```text
<Demo_Load32>:
  6800        ldr     r0, [r0, #0]
  4770        bx      lr
```

## 2. Run the narrow test

```sh
timeout 10 qemu-system-arm \
  -M mps2-an505 -cpu cortex-m33 \
  -nographic -semihosting \
  -kernel build/qemu/test_narrow.elf
```

Expected (PC value varies by toolchain, but `pc == Demo_ProbeLoad`):

```text
=== cortex-m33-boot-busfault QEMU ===
g_demo_result=3 (0=RUNNING 1=VALID_FAIL 2=RECOVERY_FAIL 3=PASSED)
g_fault.exception=5
g_fault.cfsr=0x00008200
g_fault.hfsr=0x00000000
g_fault.bfar=0x60000000
g_fault.pc=0x100005ba valid=1
live SCB->CFSR=0x00000000 HFSR=0x00000000 BFAR=0x60000000 SHCSR=0x00020000
Demo_ProbeLoad=0x100005ba
RESULT: PASS
```

What it means:

- `g_demo_result=3` (`DEMO_PASSED`): valid read worked, faulting read
  returned `false`, old value preserved, `BFAR` matched.
- `exception=5`: BusFault, not HardFault.
- `cfsr=0x8200`: `PRECISERR (0x200)` + `BFARVALID (0x8000)`.
- `hfsr=0`: never escalated to HardFault.
- `bfar=0x60000000`: matches `DEMO_FAULT_ADDRESS`.
- `live CFSR=0`: handler cleared flags with write-one-to-clear.
- `SHCSR=0x20000`: BusFault enabled (bit 17).

## 3. Build and run the wide (32-bit) test

Only the assembly changes. Rebuild that one object with
`-DDEMO_WIDE_LOAD`, relink, run:

```sh
arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -DDEMO_WIDE_LOAD \
  -c examples/probe_read.S -o build/qemu/probe_asm_wide.o

arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -nostartfiles -T test/qemu/link.ld \
  build/qemu/vectors.o build/qemu/boot.o build/qemu/probe.o \
  build/qemu/probe_asm_wide.o build/qemu/harness.o build/qemu/startup_enable.o \
  -o build/qemu/test_wide.elf -Wl,-Map=build/qemu/test_wide.map

arm-none-eabi-objdump -d build/qemu/test_wide.elf | grep -A2 "<Demo_Load32>:"

timeout 10 qemu-system-arm \
  -M mps2-an505 -cpu cortex-m33 \
  -nographic -semihosting \
  -kernel build/qemu/test_wide.elf
```

Expected encoding and same `PASS` report:

```text
<Demo_Load32>:
  f8d0 0000   ldr.w   r0, [r0]
  4770        bx      lr
```

```text
g_demo_result=3 (0=RUNNING 1=VALID_FAIL 2=RECOVERY_FAIL 3=PASSED)
g_fault.cfsr=0x00008200
g_fault.bfar=0x60000000
RESULT: PASS
```

## 4. Optional: inspect with GDB

In one terminal, freeze the CPU at reset:

```sh
qemu-system-arm \
  -M mps2-an505 -cpu cortex-m33 \
  -nographic -semihosting \
  -kernel build/qemu/test_narrow.elf \
  -S -s
```

In another terminal (repo root):

```sh
gdb-multiarch \
  -ex "set architecture armv8-m.main" \
  -ex "target remote :1234" \
  -ex "break QEMU_ReportAndExit" \
  -ex "continue" \
  -ex "print g_demo_result" \
  -ex "print g_fault" \
  -ex "print/x SCB->CFSR" \
  build/qemu/test_narrow.elf
```

If you have `arm-none-eabi-gdb`, use it instead of `gdb-multiarch`.
Expected: `g_demo_result = 3`,
`g_fault = {exception = 5, cfsr = 33280 (0x8200), hfsr = 0, ...}`.

## Troubleshooting

- No output: you dropped `-semihosting` (needed for `SYS_EXIT`) or
  `-nographic` (needed for UART0 on stdio). Keep both.
- Immediate lockup / no `Reset`: vectors linked at `0x0` instead of
  `0x10000000`. Use `test/qemu/link.ld` as-is on AN505.
- `ldr` does not fault: `DEMO_FAULT_ADDRESS` hit mapped RAM/peripherals.
  Keep `0x60000000` on AN505; it must stay 4-byte aligned.
- Duplicate log lines: old harness revisions printed via UART and
  semihosting at once. The vendored `qemu_harness.c` prints text on UART
  only and uses semihosting for exit, so each line appears once.
- `arm-none-eabi-gdb: command not found` on Debian: use `gdb-multiarch`.
