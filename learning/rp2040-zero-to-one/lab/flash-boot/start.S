    .syntax unified
    .cpu cortex-m0plus
    .thumb

/*
 * boot2가 VTOR = 0x10000100으로 두고 첫 워드를 MSP, 둘째 워드로 점프한다.
 * 시스템 예외 16칸 + IRQ 32칸(데이터시트 2.3.2 Table 80).
 * C에서 같은 이름의 함수를 정의하지 않은 핸들러는 hang을 가리킨다.
 */
    .macro default_handler name
    .weak \name
    .thumb_set \name, hang
    .endm

    .irp name, isr_nmi, isr_hardfault, isr_svcall, isr_pendsv, isr_systick
    default_handler \name
    .endr
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    default_handler isr_irq\n
    .endr

    .section .vectors, "a"
    .word __stack_top
    .word _reset
    .word isr_nmi
    .word isr_hardfault
    .word 0, 0, 0, 0, 0, 0, 0
    .word isr_svcall
    .word 0, 0
    .word isr_pendsv
    .word isr_systick
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
    .word isr_irq\n
    .endr

    .section .text._reset, "ax"
    .global _reset
    .thumb_func
_reset:
    /* .data 초기값을 flash에서 SRAM으로 복사 */
    ldr r0, =__data_load
    ldr r1, =__data_start
    ldr r2, =__data_end
data_loop:
    cmp r1, r2
    bhs data_done
    ldr r3, [r0]
    str r3, [r1]
    adds r0, r0, #4
    adds r1, r1, #4
    b data_loop
data_done:

    ldr r0, =__bss_start
    ldr r1, =__bss_end
    movs r2, #0
bss_loop:
    cmp r0, r1
    bhs bss_done
    str r2, [r0]
    adds r0, r0, #4
    b bss_loop
bss_done:

    bl main

    .thumb_func
hang:
    b hang
