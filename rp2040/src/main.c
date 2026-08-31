#define REG(addr) (*(volatile unsigned int *)(addr))

#define RESETS_BASE     0x4000c000u
#define IO_BANK0_BASE   0x40014000u
#define SIO_BASE        0xd0000000u

#define RESETS_RESET         (RESETS_BASE   + 0x00)
#define RESETS_RESET_DONE    (RESETS_BASE   + 0x08)
#define IO_BANK0_GPIO16_CTRL (IO_BANK0_BASE + 0x84)   /* 0x04 + 16*8 */
#define IO_BANK0_GPIO17_CTRL (IO_BANK0_BASE + 0x8c)   /* 0x04 + 17*8 */
#define SIO_GPIO_OE_SET      (SIO_BASE      + 0x24)
#define SIO_GPIO_OUT_SET     (SIO_BASE      + 0x14)
#define SIO_GPIO_OUT_CLR     (SIO_BASE      + 0x18)
#define SIO_GPIO_OUT_XOR     (SIO_BASE      + 0x1c)

#define RESETS_IO_BANK0   (1u << 5)   /* RESETS 안에서 IO_BANK0 의 자리 */
#define FUNCSEL_SIO       5u          /* 데이터시트 Table 279 의 F5 */
#define LED_G             16u
#define LED_R             17u

int main(void)
{
    /* IO_BANK0 의 잠금을 푼다. 나머지 24 개 블록은 건드리지 않아야 하고
       이 레지스터에는 SET/CLR 짝이 없어 읽고 고쳐 쓴다. */
    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_IO_BANK0;

    /* 해제는 즉시 끝나지 않는다. 안 기다리면 아래 설정이 날아갈 수 있다. */
    while ((REG(RESETS_RESET_DONE) & RESETS_IO_BANK0) == 0) { }

    /* 두 핀을 SIO 가 쓰도록 배정한다. 상위 필드는 리셋값이 0 이라 통째로 쓴다. */
    REG(IO_BANK0_GPIO16_CTRL) = FUNCSEL_SIO;
    REG(IO_BANK0_GPIO17_CTRL) = FUNCSEL_SIO;

    /* 출력 방향을 연다. 이걸 빼면 값을 써도 핀까지 나가지 않는다. */
    REG(SIO_GPIO_OE_SET) = (1u << LED_G) | (1u << LED_R);

    /* 둘을 반대 상태로 시작시킨다. 아래에서 동시에 뒤집으므로
       여기서 갈라놓지 않으면 같이 깜빡이게 된다. */
    REG(SIO_GPIO_OUT_SET) = 1u << LED_G;    /* HIGH = 꺼짐 */
    REG(SIO_GPIO_OUT_CLR) = 1u << LED_R;    /* LOW  = 켜짐 */

    /* XOR 은 마스크에 든 비트를 전부 뒤집는다. 현재 값을 읽을 필요가 없다.
       지연 루프의 volatile 을 빼면 컴파일러가 루프를 통째로 지운다. */
    while (1) {
        REG(SIO_GPIO_OUT_XOR) = (1u << LED_R) | (1u << LED_G);
        for (volatile int i = 0; i < 100000; i++) { }
    }
}
