#include <apm_elf_size.h>
#include <mm_elf_size.h>

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

  . = ALIGN(16);

  .apm_elf :
  {
    _apm_elf_start = .;
    KEEP(*(.apm_elf))
    . += CONFIG_APM_ELF_SIZE;
    _apm_elf_end = .;
  }

  .mm_elf :
  {
    _mm_elf_start = .;
    KEEP(*(.mm_elf))
    . += CONFIG_MM_ELF_SIZE;
    _mm_elf_end = .;
  }
}
