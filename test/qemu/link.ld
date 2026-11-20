/* AN505 secure aliases: 0x0 -> 0x10000000, 0x28000000 -> 0x38000000.
 * CPU boots Secure with SVTOR=0x10000000, so vectors/code must link there.
 * Data/stack use secure alias of SSRAM-1 to stay Secure + MPC-clean. */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 4M
  RAM   (rwx) : ORIGIN = 0x38000000, LENGTH = 2M
}

__StackTop = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .vectors 0x10000000 :
  {
    . = ALIGN(128);
    KEEP(*(.vectors))
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text.Reset_Handler)
    *(.text*)
    *(.rodata*)
    . = ALIGN(2);
  } > FLASH

  .ARM.exidx :
  {
    *(.ARM.exidx*)
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /DISCARD/ :
  {
    *(.ARM.attributes*)
  }
}
