/*
 * flash에서 부팅하는 프로그램의 링커 스크립트
 *
 * 0x10000000  boot2 256바이트. bootrom이 SRAM5로 복사해 실행한다
 * 0x10000100  벡터 테이블. boot2가 VTOR을 여기로 두고 SP, PC를 읽는다
 * 그 뒤       .text, .rodata, .data 초기값
 */
ENTRY(_reset)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

SECTIONS
{
    .boot2 : {
        KEEP(*(.boot2))
    } > FLASH

    .vectors : {
        KEEP(*(.vectors))
    } > FLASH

    .text : {
        *(.text*)
    } > FLASH

    /* 끝을 맞추지 않으면 .data 초기값의 flash 주소가 4의 배수가 아니게 되고,
       시작 코드의 ldr이 HardFault를 낸다 (M0+는 비정렬 접근 불가) */
    .rodata : {
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* 실행 주소는 SRAM, 초기값은 flash. 시작 코드가 복사한다.
       .ramfunc는 flash를 지우거나 쓰는 동안 실행할 SRAM 함수다 */
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.ramfunc*)
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > FLASH
    __data_load = LOADADDR(.data);

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
__image_end = __data_load + SIZEOF(.data);

ASSERT(SIZEOF(.boot2) == 256, "boot2 must be 256 bytes")
ASSERT(ADDR(.vectors) == ORIGIN(FLASH) + 0x100,
       "vector table must be at 0x10000100")
ASSERT(SIZEOF(.vectors) == 48 * 4, "vector table must have 16 + 32 entries")
ASSERT(__data_load % 4 == 0, ".data load address must be word aligned")
