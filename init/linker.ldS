#include <ramfs_size.h>

OUTPUT_ARCH( "riscv" )
ENTRY( _start )

SECTIONS
{
  . = CONFIG_ROOT_TASK_PAYLOAD_BASE_ADDRESS;

  .text :
  {
    KEEP(*(.text.entry))
    *(.text)
    *(.text.*)
  }

  . = ALIGN(16);

  .rodata :
  {
    *(.srodata*)
    . = ALIGN(16);
    *(.rodata)
    . = ALIGN(16);
    *(.rodata.*)
  }

  . = ALIGN(16);

  .data :
  {
    __global_pointer$ = . + 0x800;
    *(.sdata*)
    *(.data)
    *(.data.*)
    . = ALIGN(16);
    *(.sbss*)
    *(.bss)
    *(.bss.*)
    . = ALIGN(16);
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)
           SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array .ctors))
    __init_array_end = .;
    __fini_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)
           SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP(*(.fini_array .dtors))
    __fini_array_end = .;
  }

  . = ALIGN(0x1000);

  .ramfs :
  {
    _ramfs_start = .;
    KEEP(*(.ramfs))
    . += CONFIG_RAMFS_SIZE;
    _ramfs_end = .;
  }
}
