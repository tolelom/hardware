/* RAM 에서 실행하는 배치. 부트롬이 SRAM 에 적재해 바로 실행하는 경우에 쓴다.
   플래시 실행용은 별도 파일이다. */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

SECTIONS
{
    /* 벡터 테이블. CPU 가 리셋 직후 이 자리를 읽으므로 맨 앞이어야 한다.
       코드에서 참조하지 않아 --gc-sections 에 지워지므로 KEEP 이 필요하다. */
    .vectors : {
        __vectors_start = .;
        KEEP(*(.vectors*))
        __vectors_end = .;
    } > RAM

    .text : {
        __text_start = .;
        *(.text*)
        __text_end = .;
    } > RAM

    .rodata : {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    } > RAM

    /* ALIGN 은 섹션 내용이 아니라 아래 심볼의 위치를 4 의 배수로 맞추기 위한 것이다.
       시작 코드가 이 구간을 4 바이트씩 훑기 때문에, 어긋나면 Cortex-M0+ 에서
       정렬 위반으로 죽거나 종료 조건을 지나쳐 무한 루프가 된다. */
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)          /* 초기화 없이 선언된 전역 변수가 모이는 자리 */
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

/* 벡터 테이블이 bin 에서 빠지는 사고를 링크 단계에서 잡는다. 섹션 플래그 a 를
   빠뜨리거나 KEEP 을 잊으면 빌드는 통과하고 칩에서만 죽는다. 두 경우 모두
   코드가 RAM 맨 앞으로 당겨지므로 _start 의 주소로 판별한다.
   인터럽트 핸들러를 추가해 표가 커지면 아래 8 을 함께 고쳐야 한다. */
ASSERT(_start == ORIGIN(RAM) + 8,
       "vector table is missing from the image: check the section flag a and KEEP()")
ASSERT((_start & 1) == 0,
       "_start must be even; the thumb bit belongs at the reference, not the symbol")
