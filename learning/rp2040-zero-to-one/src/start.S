.syntax unified
.cpu cortex-m0plus
.thumb

.section .start, "ax"
.global _start
.thumb_func

_start:
    ldr r0, =__stack_top
    mov sp, r0

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

hang:
    b hang