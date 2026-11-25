# Cortex-M33 BusFault during boot

An ordinary faulting access during boot is fatal. When a missing device is an
expected outcome, the caller can opt in to a **single guarded 32-bit read**:

```c
uint32_t id;
if (!BusFault_TryRead32(id_register, &id)) {
    return DEVICE_ABSENT;   /* Caller chooses its fallback. */
}
/* Only this path uses id. */
```

`id_register` must be a word-aligned address and `id` must be writable. On
success the probe writes the word and returns `true`. On a precise BusFault at
its source load it leaves the output unchanged and returns `false`. Use an
ordinary volatile load instead when failure must stop boot. Do not probe an
address unless the board permits the read and its possible device side effects.

## Use in firmware

1. Copy the contents of `src/` into your project and build `boot_busfault.c` and `try_read32.S` for Cortex-M33 using a compiler that supports GNU-style inline assembly. Add the copied directory to the include path for `boot_busfault.h`. The files in `src/` do not require CMSIS or a vendor device header; only the QEMU tests use CMSIS.
2. To use the sample startup, build `src/start.S` and provide the 8-byte-aligned linker symbol `__StackTop` and `Boot_Main`. Place `.vectors` at the boot vector address with the alignment your chip requires. `Reset_Handler` enables BusFault before calling `Boot_Main`. Add the other vectors and runtime setup your chip needs.
3. To keep your vendor startup, omit `src/start.S` or build it with `-DBUSFAULT_USE_VENDOR_STARTUP` to suppress the sample startup. Point the active exception vectors to `BusFault_Handler` and `HardFault_Handler`. BusFault is exception 5, at offset `0x14`. If you already have a strong HardFault handler, combine the two handlers in your project. From privileged boot code, set bit 17 (`BUSFAULTENA`) in `SCB->SHCSR`, preserving the other bits, then execute `DSB` and `ISB` as shown in `src/start.S`. Do this after the handler stack and the RAM for `g_fault` are ready, before the first guarded read.

If recovery fails, the default `BusFault_OnFatal` masks normal interrupts and halts. To print the captured `exception`, `cfsr`, `hfsr`, `bfar`, `pc`, and `pc_valid` before halting, define `BusFault_FatalWrite(const char *text)` in your board code. The hook passes short, NUL-terminated fragments to the writer. Use a console available during boot with bounded polling and no locks or interrupt dependencies. The QEMU sample connects this hook to UART0.

Override `BusFault_OnFatal` if your board needs to put hardware in a safe state or reset. If you need the record after reset, put `g_fault` in RAM that the reset and startup code preserve.

## What the handler catches

The handler records the fault in `g_fault` for diagnostics. It accepts only a
precise data BusFault with no other fault flags (apart from `BFARVALID`), a
basic Thread-mode frame in plain Thumb state, and a saved PC equal to the
load label in [`try_read32.S`](src/try_read32.S). It redirects the saved PC to
the probe's `return false` path. It never decodes instructions or resumes at
“PC + instruction size”. The result does not depend on reading or clearing
`g_fault` at the call site.

There is no global “catch enabled” flag. Another load, the store to `*out`,
faults in an interrupt handler, and escalations to HardFault are **not**
caught. This is not a general-purpose try/catch block or a substitute for
memory protection. The handler assumes a Cortex-M33 without the Security
Extension and a working handler stack. Keep it and its hooks free of
floating-point instructions.

**Migrating:** remove the old `busfault_skip` linker section and any
`BusFault_GetFixup` override. Neither registers a recoverable site anymore.
Replace only the reads whose callers can handle failure with
`BusFault_TryRead32`. Stores have no recoverable API.

## Run the sample

[`examples/probe_read.c`](examples/probe_read.c) provides `Boot_Main`. Set
`DEMO_FAULT_ADDRESS` to a board-documented, aligned address that gives a
precise data BusFault. The sample checks a successful read, a failed read with
unchanged output, and another success. For QEMU commands and fatal-path tests,
see [docs/qemu.md](docs/qemu.md).

## Official Arm references

Comments use “Arm M33 Guide” for the Cortex-M33 Devices Generic User Guide, Issue 06.

- [Cortex-M33 Devices Generic User Guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49) — Sections 2.3.7.1 and 2.3.7.2 show the stack frame, `EXC_RETURN`, and how exception return restores the PC. Section 2.1.3.6.3 describes the Thumb, IT, and ICI bits.
- [Cortex-M33 BusFault Status Register](https://documentation-service.arm.com/static/66be2153882fec713ef49e49#page=255) — Section 4.2.11.2 explains why the saved PC points to the faulting instruction for `PRECISERR`. Sections 4.2.10 and 4.2.11 cover BusFault enable and clearing fault flags.
- [CMSIS Cortex-M33 GNU startup file](https://github.com/ARM-software/CMSIS_5/blob/develop/Device/ARM/ARMCM33/Source/GCC/startup_ARMCM33.S) — Arm's assembly vector table.
- [CMSIS Cortex-M33 core header](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h) — SCB registers and bit masks.
- [CMSIS-View Fault Storage](https://arm-software.github.io/CMSIS-View/main/group__Fault__Storage.html) — Arm's example for entering a fault handler and saving fault details.
