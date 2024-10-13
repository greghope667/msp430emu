OUTPUT_ARCH(msp430)
ENTRY(_start)

MEMORY
{
  rom  (rx) : ORIGIN = 0x0000, LENGTH = 0x1000
  ram (rwx) : ORIGIN = 0x4000, LENGTH = 0x1000
}

SECTIONS
{
  .text :
  {
    . = ALIGN(2);
    __text = .;

    *(.text.start)
    *(.text .text.*);
  } > rom

  .rodata :
  {
    . = ALIGN(2);
    __rodata = .;

    *(.rodata .rodata.*)

    . = ALIGN(2);
    __etext = .;
  } > rom

  .data :
  {
    . = ALIGN(2);
    __data = .;

    *(.data .data.*)

    . = ALIGN(2);
    __edata = .;
  } > ram AT > rom

  .bss (NOLOAD):
  {
    . = ALIGN(2);
    __bss = .;

    *(.bss .bss.* COMMON);

    . = ALIGN(2);
    __ebss = .;
  } > ram

  /DISCARD/ :
  {
    *(.MSP430.attributes)
  }

  __heap = __ebss;
  __stack = ORIGIN(ram) + LENGTH(ram);

}
