# Cortex-M33 BusFault during boot

The handler records faults during boot. A board can approve recovery for a known fault site. The test sample shows how to do this for one failed read.

| File | What it does |
| --- | --- |
| [startup.S](startup.S) | Provides the core vectors and `BusFault_Enable`. |
| [boot_busfault.c](boot_busfault.c) | Handles BusFault and HardFault and records the last fault in `g_fault`. |
| [boot_busfault.h](boot_busfault.h) | Declares the fault record, fixup, and board hooks. |
| [examples/](examples/) | Contains the test boot entry, test load, and test fixup. |

## Use in firmware

1. Build `boot_busfault.c` with your chip's CMSIS device header. The example includes Arm's generic `ARMCM33.h`.
2. Use the sample vector table or keep your vendor startup. To keep the vendor startup, build `startup.S` with `-DBUSFAULT_USE_VENDOR_STARTUP`. This builds only `BusFault_Enable`. Point the active exception vectors to `BusFault_Handler` and `HardFault_Handler`. BusFault is exception 5, at offset `0x14`. If you already have a strong HardFault handler, combine the two handlers in your project.
3. Call `BusFault_Enable` from privileged boot code after the handler stack and the RAM for `g_fault` are ready. If you use the sample vector table, provide the 8-byte-aligned linker symbol `__StackTop` and `Boot_Main`. Place `.vectors` at the boot vector address with the alignment your chip requires. Add the other vectors and runtime setup your chip needs.

The default handler records the fault and halts with normal interrupts masked. For a deployed board, override `BusFault_OnFatal` to put the hardware in a safe state and halt or reset. If you need the record after reset, put `g_fault` in RAM that the reset and startup code preserve.

## Approve recovery for a known fault site

Override `BusFault_GetFixup` in board code. Check the saved instruction address in `fault->pc` against a known fault site. Check the data address in `fault->bfar` when the fixup depends on that address. Read BFAR as valid only if `CFSR.BFARVALID` was set.

Return `true` with the even Thumb address in `resume_pc` and the value to restore in `r0` in `resume_r0`. Return `false` for any other fault site. The fixup needs to account for the effect of the skipped instruction. Keep both hooks short and avoid accesses that can fault.

The handler offers recovery for a precise data BusFault with a basic Thread-mode frame and no active IT or ICI state. It uses `EXC_RETURN` to select MSP or PSP, applies the approved fixup, and clears the captured status flags in `CFSR`. Exception return restores the new PC. HardFault and all other BusFault cases go to the fatal hook.

This code assumes a Cortex-M33 without the Security Extension and a working handler stack. Extended floating-point frames go to the fatal hook. Keep the handler and its hooks free of floating-point instructions.

## Run the test sample

1. Build [examples/probe_read.c](examples/probe_read.c) and [examples/probe_read.S](examples/probe_read.S) with the core files. The sample provides `Boot_Main` and its own `BusFault_GetFixup`.
2. Set `DEMO_FAULT_ADDRESS` when building the sample C file. Choose a 4-byte-aligned address that your chip documents as returning a bus error on read. Some unmapped addresses can return data or stall, depending on the chip.
3. Run the sample and check `g_demo_result` in a debugger. `DEMO_PASSED` means the first read worked and the faulting read returned `false` with the output value intact. The saved PC moves from `Demo_ProbeLoad` to `Demo_ProbeResume`, the instruction immediately after the 16-bit `LDR`.

## Official Arm references

- [Cortex-M33 Devices Generic User Guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49) — Sections 2.3.7.1 and 2.3.7.2 show the stack frame, `EXC_RETURN`, and how exception return restores the PC. Section 2.1.3.6.3 describes the Thumb, IT, and ICI bits.
- [Cortex-M33 BusFault Status Register](https://documentation-service.arm.com/static/66be2153882fec713ef49e49#page=255) — Section 4.2.11.2 explains why the saved PC points to the faulting instruction for `PRECISERR`. Sections 4.2.10 and 4.2.11 cover BusFault enable and clearing fault flags.
- [CMSIS Cortex-M33 GNU startup file](https://github.com/ARM-software/CMSIS_5/blob/develop/Device/ARM/ARMCM33/Source/GCC/startup_ARMCM33.S) — Arm's assembly vector table.
- [CMSIS Cortex-M33 core header](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h) — SCB registers and bit masks.
- [CMSIS-View Fault Storage](https://arm-software.github.io/CMSIS-View/main/group__Fault__Storage.html) — Arm's example for entering a fault handler and saving fault details.
