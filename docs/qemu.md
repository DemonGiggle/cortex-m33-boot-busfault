# Testing on QEMU (mps2-an505, Cortex-M33)

From the repository root, run:

```sh
python3 test/qemu/matrix/run.py
```

The runner needs `arm-none-eabi-gcc`, `arm-none-eabi-nm` and a `qemu-system-arm`
that supports `-accel tcg,one-insn-per-tb=on` (e.g. QEMU 10). On Debian/Ubuntu
the packages are `gcc-arm-none-eabi` and `qemu-system-arm`.

## What the tests check

- The sample calls `BusFault_TryRead32` three times: valid, faulting, valid.
  The failed call must leave the output unchanged. The firmware checks the
  recorded fault and the cleared status; the runner checks the first PC after
  exception return against `BusFault_Read32_Failed`. It runs on both MSP and PSP.
- Unguarded narrow and wide loads/stores, including a load from the same bad
  address as the probe, must enter the fatal hook without returning to Thread
  mode. A fault on the probe's **output store** must also be fatal.
- Disabling BusFault before calling the probe must produce a fatal, escalated
  HardFault, not a `false` result.

Each case is a separate ELF. An unexpected exception return, wrong fault
record, nonzero exit or timeout fails the suite. These tests exercise precise
faults on this board model, not all possible Cortex-M33 fault conditions.

## Build and run the sample by hand

```sh
mkdir -p build/qemu
arm-none-eabi-gcc -mcpu=cortex-m33+nodsp+nofp -mthumb -ffreestanding -Os -g \
  -Isrc -Itest/qemu/cmsis -DBUSFAULT_USE_VENDOR_STARTUP \
  -DDEMO_FAULT_ADDRESS=0x60000000U -nostdlib -T test/qemu/link.ld \
  src/start.S src/boot_busfault.c src/try_read32.S examples/probe_read.c \
  test/qemu/qemu_vectors.S test/qemu/qemu_harness.c -lgcc -o build/qemu/probe.elf

timeout 10 qemu-system-arm -M mps2-an505 -cpu cortex-m33 -nographic -semihosting \
  -kernel build/qemu/probe.elf
```

`0x60000000` is unmapped on AN505. The expected UART report ends with
`RESULT: PASS`: `g_fault.exception` is 5, `cfsr` is `0x8200` (`PRECISERR` and
`BFARVALID`), `bfar` is `0x60000000`, and the saved PC is `BusFault_Read32_Load`.
The live CFSR is zero after the handler clears it. Semihosting reports the exit
status; UART0 uses the secure alias at `0x50200000`.

`test/qemu/link.ld` places vectors at the AN505 secure boot address `0x10000000`
and RAM at `0x38000000`. The QEMU vectors initialize `.data` and `.bss` before
the sample. Neither the probe nor its handler needs a custom linker section.

## Inspect the catch with GDB

Start QEMU as above with `-S -s` added to stop at reset. In another terminal:

```sh
gdb-multiarch build/qemu/probe.elf
```

```gdb
target remote :1234
break *BusFault_Read32_Failed
continue
print g_fault
x/i $pc
```

The breakpoint is the probe's failure return, not the instruction following
the load. The output store is never executed on this path.
