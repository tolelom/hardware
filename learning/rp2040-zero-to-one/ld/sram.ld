/*
 * SRAM에서 프로그램을 실행할 때 사용하는 링커 스크립트
 */

ENTRY(_start)

MEMORY
{
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

SECTIONS
{
    .start : {
        KEEP(*(.start))
    } > SRAM

    .text : {
        *(.text*)
    } > SRAM

    .rodata : {
        *(.rodata*)
    } > SRAM

    .data : {
        . = ALIGN(4);
        *(.data*)
        . = ALIGN(4);
    } > SRAM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM
}

__stack_top = ORIGIN(SRAM) + LENGTH(SRAM);

ASSERT(_start == ORIGIN(SRAM), "_start must be at the start of SRAM")
