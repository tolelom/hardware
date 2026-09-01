/* clk_sys 를 ROSC 에서 PLL_SYS 125MHz 로 옮긴다.
 *
 * 리셋 직후 CPU 는 칩 안의 링 오실레이터로 돈다. 부품 없이 즉시 도는 대신
 * 주파수가 공정과 온도에 끌려다녀 UART 보드레이트를 만들 수 없다. 그래서
 * 보드에 붙은 수정 발진자로 옮기고 PLL 로 체배한다.
 *
 * 이 파일에 반복해서 나오는 형태가 둘이다. 요청 레지스터와 완료 레지스터가
 * 짝이라 쓰고 나면 완료를 폴링한다. 그리고 클럭 mux 의 SELECTED 는 원핫이라
 * SRC 값이 아니라 1 << SRC 와 비교한다.
 */

#include "clock.h"

#define REG(addr) (*(volatile unsigned int *)(addr))

#define RESETS_BASE    0x4000c000u
#define CLOCKS_BASE    0x40008000u
#define XOSC_BASE      0x40024000u
#define PLL_SYS_BASE   0x40028000u

#define RESETS_RESET      (RESETS_BASE + 0x00)
#define RESETS_RESET_DONE (RESETS_BASE + 0x08)
#define RESETS_PLL_SYS    (1u << 12)      /* RESETS 안에서 PLL_SYS 의 자리 */

#define XOSC_CTRL     (XOSC_BASE + 0x00)
#define XOSC_STATUS   (XOSC_BASE + 0x04)
#define XOSC_STARTUP  (XOSC_BASE + 0x0c)

#define XOSC_FREQ_RANGE_1_15MHZ  0xaa0u          /* 증폭기 구동 세기를 대역에 맞춘다 */
#define XOSC_ENABLE              (0xfabu << 12)  /* 폭주한 코드가 켜지 못하게 매직 값이다 */
#define XOSC_STABLE              (1u << 31)
#define XOSC_STARTUP_DELAY       47u             /* 256 사이클 단위. 12MHz 에서 약 1ms */

#define CLK_REF_CTRL      (CLOCKS_BASE + 0x30)
#define CLK_REF_SELECTED  (CLOCKS_BASE + 0x38)
#define CLK_SYS_CTRL      (CLOCKS_BASE + 0x3c)
#define CLK_SYS_SELECTED  (CLOCKS_BASE + 0x44)

#define CLK_REF_SRC_XOSC  2u   /* CLK_REF_CTRL 의 SRC 값 */
#define CLK_SYS_SRC_AUX   1u   /* CLK_SYS_CTRL 의 SRC 값. AUXSRC 리셋값이 PLL_SYS 다 */

#define PLL_CS            (PLL_SYS_BASE + 0x00)
#define PLL_PWR           (PLL_SYS_BASE + 0x04)
#define PLL_FBDIV_INT     (PLL_SYS_BASE + 0x08)
#define PLL_PRIM          (PLL_SYS_BASE + 0x0c)

#define PLL_CS_LOCK       (1u << 31)
#define PLL_PWR_PD        (1u << 0)   /* PD 는 Power Down 이라 비트를 내려야 켜진다 */
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD     (1u << 5)

/* 12MHz x FBDIV / (POSTDIV1 x POSTDIV2) = 12 x 125 / 12 = 125MHz.
   VCO 가 750~1600MHz 에서만 돌기 때문에 1500MHz 까지 올렸다가 뒷단에서 내린다. */
#define PLL_REFDIV        1u
#define PLL_FBDIV         125u
#define PLL_POSTDIV1      6u
#define PLL_POSTDIV2      2u
#define PLL_POSTDIV1_LSB  16u
#define PLL_POSTDIV2_LSB  12u

void clock_start_xosc(void) {
    /* 정지한 수정을 정상 진폭까지 흔드는 데 시간이 걸린다. 그 시간을 먼저
       알려줘야 하드웨어가 그만큼 세고 나서 STABLE 을 세운다. */
    REG(XOSC_STARTUP) = XOSC_STARTUP_DELAY;

    /* 구동 세기와 켜기를 한 번에 쓴다. 나눠 쓰면 첫 쓰기에서 ENABLE 필드가
       0 이 되어 이미 돌고 있던 발진기가 잠깐 멎는다. */
    REG(XOSC_CTRL) = XOSC_FREQ_RANGE_1_15MHZ | XOSC_ENABLE;

    /* USB 부트로 들어오면 부트롬이 이미 켜둔 뒤라 즉시 통과한다. 플래시
       부팅으로 옮기면 여기가 실제로 기다리는 자리가 된다. */
    while ((REG(XOSC_STATUS) & XOSC_STABLE) == 0) {
    }

    /* glitchless mux 라 전환이 즉시 끝나지 않는다. 새 소스가 돌고 있지 않으면
       영영 안 끝나므로 STABLE 확인이 반드시 앞에 와야 한다. */
    REG(CLK_REF_CTRL) = CLK_REF_SRC_XOSC;
    while (REG(CLK_REF_SELECTED) != (1u << CLK_REF_SRC_XOSC)) {
    }
}

void clock_start_pll_sys(void) {
    /* 부트롬은 PLL_USB 만 쓰므로 PLL_SYS 는 리셋에 잠겨 있다. 이 레지스터에는
       SET/CLR 짝이 없어 다른 블록을 건드리지 않으려면 읽고 고쳐 쓴다. */
    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_PLL_SYS;
    while ((REG(RESETS_RESET_DONE) & RESETS_PLL_SYS) == 0) {
    }

    /* 배율을 먼저 정한다. VCO 를 FBDIV 로 나눈 것이 참조와 같아지는 지점이
       목표 주파수다. */
    REG(PLL_CS) = PLL_REFDIV;
    REG(PLL_FBDIV_INT) = PLL_FBDIV;

    /* VCO 와 참조부만 켠다. 뒷단은 아직 꺼둔다. */
    REG(PLL_PWR) = REG(PLL_PWR) & ~(PLL_PWR_PD | PLL_PWR_VCOPD);

    /* 되먹임 루프가 수렴할 때까지 기다린다. 배율이 범위 밖이거나 참조가 없으면
       영영 안 잠긴다. */
    while ((REG(PLL_CS) & PLL_CS_LOCK) == 0) {
    }

    /* 잠긴 뒤에 분주비를 정하고, 정한 뒤에 전원을 넣는다. 순서를 바꾸면 아직
       정해지지 않은 분주비로 출력이 잠깐 나간다. */
    REG(PLL_PRIM) = (PLL_POSTDIV1 << PLL_POSTDIV1_LSB) |
                    (PLL_POSTDIV2 << PLL_POSTDIV2_LSB);
    REG(PLL_PWR) = REG(PLL_PWR) & ~PLL_PWR_POSTDIVPD;

    /* AUXSRC 는 같은 쓰기에서 리셋값 0(PLL_SYS)이 유지되므로 실제로 바뀌지
       않는다. SRC 가 aux 를 보는 중에 AUXSRC 를 바꾸는 상황이 안 생긴다. */
    REG(CLK_SYS_CTRL) = CLK_SYS_SRC_AUX;
    while (REG(CLK_SYS_SELECTED) != (1u << CLK_SYS_SRC_AUX)) {
    }
}
