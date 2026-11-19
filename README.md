# Cortex-M33 BusFault during boot

This example shows how to catch a BusFault during boot and continue after a failed test read. It assumes a Cortex-M33 without the Security Extension (TrustZone).

| File | What it does |
| --- | --- |
| [startup.S](startup.S) | Sets up the core vectors, enables BusFault, and provides the test read. |
| [boot_busfault.c](boot_busfault.c) | Saves fault registers and changes the exception frame for the test read. |

## Boot flow

1. At reset, the CPU loads the Main Stack Pointer (MSP) and `Reset_Handler` from the vector table. BusFault is exception 5, at offset `0x14`.
2. `Reset_Handler` sets `SHCSR.BUSFAULTENA` and calls `Boot_Main`.
3. In `Boot_Main`, call `Boot_TryRead32` for an aligned address that your board allows you to probe.

```c
#include <stdbool.h>
#include <stdint.h>

bool Boot_TryRead32(const volatile uint32_t *address, uint32_t *value);
```

On success, the function stores the word at `value` and returns `true`. If the test load causes a precise BusFault, the function returns `false` and leaves `*value` as it was.

## How recovery works

1. The handler reads `EXC_RETURN` from `LR` and uses bit 2 to find the stack frame on MSP or PSP.
2. It checks for a precise data BusFault and a basic frame from Thread mode. It also checks that the saved PC matches `Boot_ProbeLoad`, the 16-bit `LDR` in `startup.S`.
3. It sets the saved `r3` to zero, writes `Boot_ProbeResume` to the saved PC, and clears the saved fault flags in `CFSR`. Exception return restores the new PC. The next instruction sees `r3` is zero and skips the output store.

Any other fault stops in the default loop. If BusFault escalates to HardFault, the HardFault handler saves the registers and stops. `g_fault.cfsr` keeps the original status bits for the debugger. `g_fault.bfar` is the data address when `CFSR.BFARVALID` is set. The saved PC gives the instruction address.

## Board setup

- Keep `.vectors` at the boot vector address and align it as required by your chip. Add the chip's external interrupt entries and provide the 8-byte-aligned `__StackTop` symbol.
- Use your chip's CMSIS header and provide `Boot_Main`. The stack and RAM for `g_fault` must be ready when the C handler runs. Add the C runtime setup your boot code needs.
- The example recovers with a basic exception frame. Extended floating-point frames and TrustZone stack state need extra handling.

## Official Arm references

- [Cortex-M33 Devices Generic User Guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49) — Sections 2.3.7.1 and 2.3.7.2 show the stack frame layout, `EXC_RETURN` fields, and how exception return restores the PC. These rules are the basis for the recovery code in this repo.
- [Cortex-M33 BusFault Status Register](https://documentation-service.arm.com/static/66be2153882fec713ef49e49#page=255) — Section 4.2.11.2 explains `PRECISERR` and why its saved PC points to the faulting instruction. Sections 4.2.10 and 4.2.11 cover BusFault enable and clearing fault flags.
- [CMSIS Cortex-M33 GNU startup file](https://github.com/ARM-software/CMSIS_5/blob/develop/Device/ARM/ARMCM33/Source/GCC/startup_ARMCM33.S) — Arm's assembly vector table.
- [CMSIS Cortex-M33 core header](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h) — SCB registers and bit masks.
- [CMSIS-View Fault Storage](https://arm-software.github.io/CMSIS-View/main/group__Fault__Storage.html) — Arm's example for entering a fault handler and saving fault details.
