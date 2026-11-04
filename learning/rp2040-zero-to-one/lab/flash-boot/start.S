    .syntax unified
    .cpu cortex-m0plus
    .thumb

/*
 * boot2가 VTOR = 0x10000100으로 두고 첫 워드를 MSP, 둘째 워드로 점프한다.
 * NMI, HardFault는 hang에 멈춰 두어 디버거로 위치를 볼 수 있게 한다.
 */
    .section .vectors, "a"
    .word __stack_top
    .word _reset
    .word hang
    .word hang

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
