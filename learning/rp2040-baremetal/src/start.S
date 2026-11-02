/* RAM 실행용 시작 코드. 0 단계라 아직 아무 주변장치도 건드리지 않는다. */

.cpu cortex-m0plus      /* Cortex-M0+ 에 없는 명령을 조립 단계에서 걸러낸다 */
.thumb                  /* 이 코어는 Thumb 인코딩만 실행한다 */

/* CPU 가 리셋 직후 이 표의 앞 두 워드를 읽어 sp 와 pc 에 넣는다.
   따라서 이미지 맨 앞이어야 하고, 링커스크립트가 .vectors 를 첫 섹션에 둔다.
   "a" 를 붙여야 메모리에 올라가는 섹션으로 표시되어 objcopy 가 bin 에 담는다. */
.section .vectors, "a"
    .word __stack_top
    .word _start        /* .thumb_func 덕분에 최하위 비트가 자동으로 선다 */

.section .text
.global _start
.thumb_func             /* 이 심볼을 참조할 때 Thumb 표시 비트를 어셈블러가 붙인다.
                           직접 +1 을 적으면 빠뜨려도 빌드가 통과하고 칩에서만 죽는다 */
_start:
    /* Thumb 의 ldr 은 sp 를 직접 목적지로 잡지 못해 두 단계로 나눈다.
       하드웨어 리셋이면 CPU 가 위 표를 읽어 이미 세팅하지만, 부트롬이
       점프해 들어오는 경로에서는 그 보장이 없어 여기서 다시 세운다. */
    ldr r0, = __stack_top
    mov sp, r0
    bl main
hang:
    b hang
