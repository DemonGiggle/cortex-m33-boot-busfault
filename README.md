# Cortex-M33 BusFault during boot

This small example shows how to catch a BusFault during early boot. It uses CMSIS register names and assumes a Cortex-M33 without the Security Extension (TrustZone).

## Boot flow

1. At reset, the CPU loads the initial Main Stack Pointer (MSP) and the address of `Reset_Handler` from the vector table. The BusFault entry is exception 5, at offset `0x14`.
2. `Reset_Handler` sets `SHCSR.BUSFAULTENA` before it calls `Boot_Main`. This bit lets the CPU enter `BusFault_Handler`.
3. The handler saves the fault registers in `g_fault` and stays in a loop. Halt the CPU with a debugger and inspect `g_fault`.

See [boot_busfault.c](boot_busfault.c) for the vector table, reset code, and handlers.

## What to check in the debugger

| Field | What to look at |
| --- | --- |
| `g_fault.cfsr` | Bits 15:8 contain the BusFault Status Register (BFSR). `PRECISERR` means a precise data bus fault. `STKERR` means a fault while saving the exception stack frame. |
| `g_fault.bfar` | This is the fault address only when `BFARVALID` (CFSR bit 15) is set. Address zero can be valid. |
| `g_fault.hfsr` | `FORCED` (bit 30) means a configurable fault became a HardFault. Check CFSR for the original cause. A disabled or masked BusFault can become a HardFault. |

## Board setup

- Use your chip's CMSIS device header. `ARMCM33.h` is Arm's generic example.
- Keep `.vectors` at the boot vector address in your linker script. Use the alignment required by your chip and add its external interrupt entries. The first table entry must point to a valid, writable stack.
- Provide `Boot_Main` and the linker symbol `__StackTop`. The stack and RAM used by `g_fault` must work before the C handler can save a fault. Add your board's C runtime setup if your boot code needs initialized data.
- If you use TrustZone, check where BusFault is routed by `AIRCR.BFHFNMINS` and use the vector table for that security state.

## Official Arm references

- [Cortex-M33 Devices Generic User Guide](https://documentation-service.arm.com/static/66be2153882fec713ef49e49) — vector table (§2.3.4), fault handling (§2.5), and fault registers (§4.2.10–4.2.14).
- [CMSIS Cortex-M33 startup file](https://github.com/ARM-software/CMSIS_5/blob/develop/Device/ARM/ARMCM33/Source/startup_ARMCM33.c) — Arm's C vector table. The table in this repo follows its core exception order.
- [CMSIS Cortex-M33 core header](https://github.com/ARM-software/CMSIS_6/blob/main/CMSIS/Core/Include/core_cm33.h) — definitions for the SCB registers and bit masks used in the example.
- [CMSIS-View Fault Storage](https://arm-software.github.io/CMSIS-View/main/group__Fault__Storage.html) — Arm's `BusFault_Handler` example branches to `ARM_FaultSave` to save more detail without using a stack. This repo uses a short C handler to show the register reads directly.
