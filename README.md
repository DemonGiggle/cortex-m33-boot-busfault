# Cortex-M33 BusFault during boot

The handler records faults during boot. For a precise data fault in designated boot code, it skips one Thumb instruction and continues. The sample exercises the default handler with a failed read.

| File | What it does |
| --- | --- |
| [startup.S](startup.S) | Provides the core vectors and `BusFault_Enable`. |
| [boot_busfault.c](boot_busfault.c) | Handles BusFault and HardFault and records the last fault in `g_fault`. |
| [boot_busfault.h](boot_busfault.h) | Declares the fault record, fixup, and board hooks. |
| [examples/](examples/) | Contains the test boot entry and test load. |
| [test/qemu/](test/qemu/) | QEMU-only vectors, linker script, harness, and CMSIS for `mps2-an505`. |
| [docs/qemu.md](docs/qemu.md) | Step-by-step QEMU run (narrow + wide, expected output). |

## Use in firmware

1. Build `boot_busfault.c` with your chip's CMSIS device header. The example includes Arm's generic `ARMCM33.h`.
2. Use the sample vector table or keep your vendor startup. To keep the vendor startup, build `startup.S` with `-DBUSFAULT_USE_VENDOR_STARTUP`. This builds only `BusFault_Enable`. Point the active exception vectors to `BusFault_Handler` and `HardFault_Handler`. BusFault is exception 5, at offset `0x14`. If you already have a strong HardFault handler, combine the two handlers in your project.
3. Call `BusFault_Enable` from privileged boot code after the handler stack and the RAM for `g_fault` are ready. If you use the sample vector table, provide the 8-byte-aligned linker symbol `__StackTop` and `Boot_Main`. Place `.vectors` at the boot vector address with the alignment your chip requires. Add the other vectors and runtime setup your chip needs.

If recovery fails, the default fatal hook halts with normal interrupts masked. For a deployed board, override `BusFault_OnFatal` to put the hardware in a safe state and halt or reset. If you need the record after reset, put `g_fault` in RAM that the reset and startup code preserve.

## Skip a faulting instruction

Place a routine in the executable section `busfault_skip` only if its caller can handle a dropped access. Put the instruction that follows the access in the same section. The handler must be able to read instruction bytes there as data. Choose byte-readable flash or RAM. Keep stack operations, multiple transfers, and operations with required side effects outside this section.

In a GNU linker script, place this output section after boot `.text` in byte-readable flash. Use the flash region name from your board.

```ld
busfault_skip ALIGN(2) :
{
    __start_busfault_skip = .;
    KEEP(*(busfault_skip))
    __stop_busfault_skip = .;
} > FLASH
```

1. The handler accepts a precise data BusFault with a basic Thread-mode frame in plain Thumb state. It checks the status before reading the saved PC. Stacking faults, other fault flags, and active IT or ICI state go to the fatal hook.
2. `BusFault_GetFixup` checks that the PC is inside the section. It reads the first halfword's high byte. Prefixes `11101`, `11110`, and `11111` give a 4-byte instruction. Other prefixes give a 2-byte instruction.
3. The handler writes `PC + size` to the saved PC and clears the captured fault flags. Exception return resumes at that address. The default fixup preserves the stacked general registers. The caller must discard the result of a failed load. A skipped store supplies no completion guarantee.

The bounds are optional weak symbols. With an empty or missing section, the default fixup declines recovery. A board can still override `BusFault_GetFixup` to supply a known resume address and a result in `resume_r0`, including for execute-only code. Arm documents the instruction encoding and exception return. Skipping an access is the firmware policy used here.

This code assumes a Cortex-M33 without the Security Extension and a working handler stack. Extended floating-point frames go to the fatal hook. Keep the handler and its hooks free of floating-point instructions.

## Run the test sample

To run on QEMU with copy-paste commands, see [docs/qemu.md](docs/qemu.md).

1. Build [examples/probe_read.c](examples/probe_read.c) and [examples/probe_read.S](examples/probe_read.S) with the core files and the section placement above. The sample supplies `Boot_Main` and uses the production `BusFault_GetFixup`.
2. Set `DEMO_FAULT_ADDRESS` when building the sample C file. Choose a 4-byte-aligned address that your chip documents as returning a bus error on read.
3. Run the sample and inspect `g_demo_result`. `DEMO_PASSED` means a valid read worked, the faulting read returned `false`, its output word kept its previous value, and BFAR identified the test address. In the debugger, exception return lands on the `BX LR` after the load.
4. For the 32-bit encoding, rebuild the sample assembly with `-DDEMO_WIDE_LOAD`. The default build uses a 16-bit load.

## Official Arm references

Comments use “Arm M33 Guide” for the Cortex-M33 Devices Generic User Guide, Issue 06.

- [Armv8-M Architecture Reference Manual, DDI 0553](https://developer.arm.com/documentation/ddi0553/latest/) — In revision B.n, section C2.1 gives the T32 width prefixes. Section B6.3 describes instruction byte order.
- [Cortex-M33 Devices Generic User Guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49) — Sections 2.3.7.1 and 2.3.7.2 show the stack frame, `EXC_RETURN`, and how exception return restores the PC. Section 2.1.3.6.3 describes the Thumb, IT, and ICI bits.
- [Cortex-M33 BusFault Status Register](https://documentation-service.arm.com/static/66be2153882fec713ef49e49#page=255) — Section 4.2.11.2 explains why the saved PC points to the faulting instruction for `PRECISERR`. Sections 4.2.10 and 4.2.11 cover BusFault enable and clearing fault flags.
- [CMSIS Cortex-M33 GNU startup file](https://github.com/ARM-software/CMSIS_5/blob/develop/Device/ARM/ARMCM33/Source/GCC/startup_ARMCM33.S) — Arm's assembly vector table.
- [CMSIS Cortex-M33 core header](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h) — SCB registers and bit masks.
- [CMSIS-View Fault Storage](https://arm-software.github.io/CMSIS-View/main/group__Fault__Storage.html) — Arm's example for entering a fault handler and saving fault details.
