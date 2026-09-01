#include "clock.h"
#include "uart.h"

#define REG(addr) (*(volatile unsigned int *)(addr))

#define RESETS_BASE     0x4000c000u
#define IO_BANK0_BASE   0x40014000u
#define SIO_BASE        0xd0000000u

#define RESETS_RESET         (RESETS_BASE   + 0x00)
#define RESETS_RESET_DONE    (RESETS_BASE   + 0x08)
#define IO_BANK0_GPIO16_CTRL (IO_BANK0_BASE + 0x84)   /* 0x04 + 16*8 */
#define IO_BANK0_GPIO17_CTRL (IO_BANK0_BASE + 0x8c)   /* 0x04 + 17*8 */
#define IO_BANK0_GPIO25_CTRL (IO_BANK0_BASE + 0xcc)   /* 0x04 + 25*8 */
#define SIO_GPIO_OE_SET      (SIO_BASE      + 0x24)
#define SIO_GPIO_OUT_SET     (SIO_BASE      + 0x14)
#define SIO_GPIO_OUT_CLR     (SIO_BASE      + 0x18)
#define SIO_GPIO_OUT_XOR     (SIO_BASE      + 0x1c)

#define RESETS_IO_BANK0   (1u << 5)   /* RESETS 안에서 IO_BANK0 의 자리 */
#define FUNCSEL_SIO       5u          /* 데이터시트 Table 279 의 F5 */
#define LED_G             16u
#define LED_R             17u
#define LED_B             25u

/* 깜빡임과 로그의 간격. 125MHz 기준으로 눈에 보이는 정도로 잡았다. */
#define BLINK_DELAY       10000000u

int main(void) {
    /* IO_BANK0 의 잠금을 푼다. 나머지 24 개 블록은 건드리지 않아야 하고
       이 레지스터에는 SET/CLR 짝이 없어 읽고 고쳐 쓴다. */
    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_IO_BANK0;

    /* 해제는 즉시 끝나지 않는다. 안 기다리면 아래 설정이 날아갈 수 있다. */
    while ((REG(RESETS_RESET_DONE) & RESETS_IO_BANK0) == 0) {
    }

    /* 세 핀을 SIO 가 쓰도록 배정한다. 상위 필드는 리셋값이 0 이라 통째로 쓴다.
       파랑은 안 쓰더라도 배정해야 한다. 놔두면 리셋값 풀다운으로 희미하게
       켜져서 다른 LED 판독을 흐린다. */
    REG(IO_BANK0_GPIO16_CTRL) = FUNCSEL_SIO;
    REG(IO_BANK0_GPIO17_CTRL) = FUNCSEL_SIO;
    REG(IO_BANK0_GPIO25_CTRL) = FUNCSEL_SIO;

    /* 출력 방향을 연다. 이걸 빼면 값을 써도 핀까지 나가지 않는다. */
    REG(SIO_GPIO_OE_SET) = (1u << LED_G) | (1u << LED_R) | (1u << LED_B);

    /* clk_sys 를 ROSC 에서 PLL_SYS 125MHz 로 옮긴다. UART 보드레이트가 이
       클럭의 분주에서 나오므로 UART 초기화보다 반드시 앞에 와야 한다. */
    clock_start_xosc();
    clock_start_pll_sys();

    uart_init();
    uart_puts("rp2040 baremetal\r\n");

    /* 파랑은 쓰지 않는다. OUT 리셋값이 0(LOW)이라 배정만 해두면 켜진 채로 남는다.
       초록은 아래에서 빨강과 반대로 깜빡이도록 여기서 갈라놓는다. */
    REG(SIO_GPIO_OUT_SET) = (1u << LED_G) | (1u << LED_B);  /* HIGH = 꺼짐 */
    REG(SIO_GPIO_OUT_CLR) = 1u << LED_R;                    /* LOW  = 켜짐 */

    /* LED 는 UART 가 안 나올 때 코드가 도는지 보는 최후 수단으로 남긴다.
       XOR 은 마스크에 든 비트를 뒤집으므로 현재 값을 읽을 필요가 없다.
       지연 루프의 volatile 을 빼면 컴파일러가 루프를 통째로 지운다. */
    while (1) {
        REG(SIO_GPIO_OUT_XOR) = (1u << LED_R) | (1u << LED_G);
        uart_puts("hello\r\n");
        for (volatile unsigned int i = 0; i < BLINK_DELAY; i++) {
        }
    }
}
