#include "clock.h"

#define REG(addr) (*(volatile unsigned int *)(addr))

#define XOSC_BASE     0x40024000u
#define CLOCKS_BASE   0x40008000u

#define XOSC_CTRL     (XOSC_BASE   + 0x00)
#define XOSC_STATUS   (XOSC_BASE   + 0x04)
#define XOSC_STARTUP  (XOSC_BASE   + 0x0c)

#define CLK_REF_CTRL      (CLOCKS_BASE + 0x30)
#define CLK_REF_SELECTED  (CLOCKS_BASE + 0x38)

#define XOSC_FREQ_RANGE_1_15MHZ  0xaa0u
#define XOSC_ENABLE              (0xfabu << 12)
#define XOSC_STABLE              (1u << 31)
#define XOSC_STARTUP_DELAY       47u

#define RESETS_BASE       0x4000c000u
#define PLL_SYS_BASE      0x40028000u

#define RESETS_RESET      (RESETS_BASE + 0x00)
#define RESETS_RESET_DONE (RESETS_BASE + 0x08)
#define RESETS_PLL_SYS    (1u << 12)

#define PLL_CS            (PLL_SYS_BASE + 0x00)
#define PLL_PWR           (PLL_SYS_BASE + 0x04)
#define PLL_FBDIV_INT     (PLL_SYS_BASE + 0x08)
#define PLL_PRIM          (PLL_SYS_BASE + 0x0c)

#define PLL_CS_LOCK       (1u << 31)
#define PLL_PWR_PD        (1u << 0)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD     (1u << 5)

#define CLK_SYS_CTRL      (CLOCKS_BASE + 0x3c)
#define CLK_SYS_SELECTED  (CLOCKS_BASE + 0x44)


void clock_start_xosc(void) {
    REG(XOSC_STARTUP) = XOSC_STARTUP_DELAY;

    REG(XOSC_CTRL) = XOSC_FREQ_RANGE_1_15MHZ | XOSC_ENABLE;

    while ((REG(XOSC_STATUS) & XOSC_STABLE) == 0) {
    }

    REG(CLK_REF_CTRL) = 2u;
    while (REG(CLK_REF_SELECTED) != (1u << 2)) {
    }
}

void clock_start_pll_sys(void) {
    REG(RESETS_RESET) = REG(RESETS_RESET) & ~RESETS_PLL_SYS;

    while ((REG(RESETS_RESET_DONE) & RESETS_PLL_SYS) == 0) {
    }

    REG(PLL_CS) = 1u; /* REFDIV = 1. 참조를 12MHz 그대로 */
    REG(PLL_FBDIV_INT) = 125u; /* VCO = 12 × 125 = 1500MHz */

    REG(PLL_PWR) = REG(PLL_PWR) & ~(PLL_PWR_PD | PLL_PWR_VCOPD);

    while ((REG(PLL_CS) & PLL_CS_LOCK) == 0) {
    }
    
    REG(PLL_PRIM) = (6u << 16) | (2u << 12);
    REG(PLL_PWR) = REG(PLL_PWR) & ~PLL_PWR_POSTDIVPD;

    REG(CLK_SYS_CTRL) = 1u;
    while (REG(CLK_SYS_SELECTED) != (1u << 1)) {
    }
}
