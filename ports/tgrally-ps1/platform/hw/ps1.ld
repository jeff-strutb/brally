/* ps1.ld: a PS-EXE in the retail console's 2 MB.  The BIOS keeps the first
 * 64 KB; the image loads at 0x80010000; the stack grows down from the top
 * (RAM_TOP, 8 MB on a dev-kit model). */
OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(_start)

__ram_top = DEFINED(RAM_TOP) ? RAM_TOP : 0x80200000;
STACK_SIZE = DEFINED(STACK_SIZE) ? STACK_SIZE : 0x8000;

MEMORY { ram (rwx) : ORIGIN = 0x80010000, LENGTH = 0x7F0000 }

SECTIONS
{
    .text : {
        __text_start = .;
        KEEP(*(.text.start))
        *(.text .text.*)
        . = ALIGN(4);
    } > ram
    .rodata : { *(.rodata .rodata.*) . = ALIGN(4); } > ram
    .data : {
        *(.data .data.*)
        . = ALIGN(8);
        _gp = . + 0x7FF0;
        *(.sdata .sdata.*)
        . = ALIGN(4);
        __data_end = .;
    } > ram
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.sbss .sbss.*) *(.scommon)
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > ram
    __heap_start = ALIGN(16);
    __stack_top = __ram_top - 0x10;
    __heap_end = __ram_top - STACK_SIZE;
    ASSERT(__heap_start <= __heap_end, "the image does not fit the console's RAM")
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.comment) *(.note*) *(.pdr) *(.gnu.attributes) }
}
