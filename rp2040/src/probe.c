/* 임시 진단 코드. 부트롬이 넘겨준 시점에 clk_sys 가 어디서 오는지 LED 로 읽는다.
   2 단계 기준선을 확정하면 이 파일과 main 의 호출을 함께 지운다. */

#include "probe.h"

#define REG(addr) (*(volatile unsigned int *)(addr))

#define CLOCKS_BASE   0x40008000u
#define XOSC_BASE     0x40024000u
#define SIO_BASE      0xd0000000u

#define CLK_REF_SELECTED  (CLOCKS_BASE + 0x38)
#define CLK_SYS_CTRL      (CLOCKS_BASE + 0x3c)
#define CLK_SYS_SELECTED  (CLOCKS_BASE + 0x44)
#define XOSC_STATUS       (XOSC_BASE   + 0x04)

#define SIO_GPIO_OUT_SET  (SIO_BASE + 0x14)
#define SIO_GPIO_OUT_CLR  (SIO_BASE + 0x18)

#define LED_G  (1u << 16)
#define LED_R  (1u << 17)
#define LED_B  (1u << 25)

/* LOW 가 켜짐이다. */
#define LED_ON(mask)   REG(SIO_GPIO_OUT_CLR) = (mask)
#define LED_OFF(mask)  REG(SIO_GPIO_OUT_SET) = (mask)

#define XOSC_STABLE   (1u << 31)

/* clk_sys 의 출처를 한 자리 숫자로 접는다.
   1 ROSC   2 XOSC   3 PLL_SYS   4 PLL_USB   5 그 밖 */
static unsigned int clk_sys_source(void) {
    unsigned int sys = REG(CLK_SYS_SELECTED);

    /* SELECTED 는 원핫이다. 비트 0 은 SRC=0, 즉 clk_ref 를 그대로 쓰는 경우다. */
    if (sys & (1u << 0)) {
        unsigned int ref = REG(CLK_REF_SELECTED);
        if (ref & (1u << 0)) return 1;
        if (ref & (1u << 2)) return 2;
        return 5;
    }

    /* 비트 1 은 SRC=1, aux 경로다. 어느 aux 인지는 CTRL 의 AUXSRC 가 쥐고 있다. */
    if (sys & (1u << 1)) {
        unsigned int aux = (REG(CLK_SYS_CTRL) >> 5) & 0x7u;
        if (aux == 0) return 3;
        if (aux == 1) return 4;
        return 5;
    }

    return 5;
}

static void spin(unsigned int n) {
    for (volatile unsigned int i = 0; i < n; i++) { }
}

void probe_clock_source(void) {
    /* clk_sys 주파수를 모르니 지연은 절대 시간이 아니다. 빨강이 몇 번
       깜빡이는지만 세면 되므로 빠르든 느리든 판정에는 지장이 없다. */
    const unsigned int SHORT = 4000000u;
    const unsigned int LONG  = 12000000u;

    unsigned int code = clk_sys_source();

    /* 셋 다 알려진 상태로 내려놓고 시작한다. 안 그러면 리셋값 풀다운으로
       희미하게 켜진 LED 를 결과로 오독하게 된다. 파랑도 여기서 꺼진다면
       빨강·초록과 같은 극성(LOW 가 켜짐)이라는 뜻이다. */
    LED_OFF(LED_R | LED_G | LED_B);

    /* 초록은 XOSC 가 떠 있는지를 그대로 보여준다. 빨강이 깜빡이는 동안 계속
       켜져 있으므로 풀다운 때문에 희미하게 켜진 것과 구별된다. */
    if (REG(XOSC_STATUS) & XOSC_STABLE) {
        LED_ON(LED_G);
    } else {
        LED_OFF(LED_G);
    }

    while (1) {
        for (unsigned int i = 0; i < code; i++) {
            LED_ON(LED_R);
            spin(SHORT);
            LED_OFF(LED_R);
            spin(SHORT);
        }
        spin(LONG);
    }
}
