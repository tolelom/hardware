/*
 * watchdog 부팅 (데이터시트 2.8.1.1). SCRATCH4에 0xb007c0d3, SCRATCH5에 entry ^ -0xb007c0d3,
 * SCRATCH6에 SP, SCRATCH7에 entry를 두고 watchdog로 리셋하면 bootrom이 flash 부팅 대신
 * entry로 간다.
 *
 * 1회째 (flash 부팅): SRAM 함수 sram_entry를 entry로 적고 리셋한다.
 * sram_entry: XIP가 꺼진 상태라 flash의 코드·상수를 쓰지 않는다. 그 순간의 상태를
 * scratch에 적고, SCRATCH4는 bootrom이 0으로 지웠으므로 다시 리셋하면 일반 flash 부팅이 된다.
 * 2회째 (flash 부팅): scratch에 남은 값을 보낸다.
 */
#include "board.h"

#define WATCHDOG_BASE 0x40058000u
#define WATCHDOG_CTRL REG(WATCHDOG_BASE + 0x00u)
#define WATCHDOG_REASON REG(WATCHDOG_BASE + 0x08u)
#define WATCHDOG_SCRATCH(n) REG(WATCHDOG_BASE + 0x0cu + (n) * 4u)
#define WATCHDOG_CTRL_ENABLE (1u << 30)
#define WATCHDOG_CTRL_TRIGGER (1u << 31)
#define PSM_WDSEL REG(0x40010000u + 0x08u)
#define PSM_WDSEL_NO_OSC 0x1fffcu /* ROSC, XOSC를 뺀 전부 */
#define BOOT_MAGIC 0xb007c0d3u
#define MARK 0x5a4d0001u
#define SRAM_STACK_TOP 0x20040000u

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

RAMFUNC static void sram_entry(void) {
    uint32_t sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    WATCHDOG_SCRATCH(1) = WATCHDOG_SCRATCH(4);
    WATCHDOG_SCRATCH(2) = PSM_WDSEL;
    WATCHDOG_SCRATCH(3) = sp;
    WATCHDOG_SCRATCH(5) = CLK_SYS_SELECTED | (CLK_REF_SELECTED << 8);
    WATCHDOG_SCRATCH(6) = WATCHDOG_REASON;
    WATCHDOG_SCRATCH(7) = SIO_CPUID;
    WATCHDOG_SCRATCH(0) = MARK;
    PSM_WDSEL = PSM_WDSEL_NO_OSC;
    WATCHDOG_CTRL = WATCHDOG_CTRL_TRIGGER;
    while (1) {
    }
}

static void print_hex(const char *label, uint32_t value) {
    uart_puts(label);
    uart_puts("0x");
    uart_puthex(value, 8);
    uart_puts("\r\n");
}

int main(void) {
    board_init();
    uint32_t reason = WATCHDOG_REASON;
    uart_puts("watchdog boot, reason 0x");
    uart_puthex(reason, 1);
    uart_puts("\r\n");

    if (WATCHDOG_SCRATCH(0) == MARK) {
        WATCHDOG_SCRATCH(0) = 0;
        print_hex("sram_entry ran at      ", (uint32_t) sram_entry);
        print_hex("  SCRATCH4 seen        ", WATCHDOG_SCRATCH(1));
        print_hex("  PSM_WDSEL seen       ", WATCHDOG_SCRATCH(2));
        print_hex("  SP seen              ", WATCHDOG_SCRATCH(3));
        print_hex("  SYS|REF<<8 SELECTED  ", WATCHDOG_SCRATCH(5));
        print_hex("  WATCHDOG_REASON seen ", WATCHDOG_SCRATCH(6));
        print_hex("  CPUID seen           ", WATCHDOG_SCRATCH(7));
        for (int i = 5; i < 8; i++) {
            WATCHDOG_SCRATCH(i) = 0;
        }
        uart_puts("done\r\n");
        while (1) {
        }
    }

    uint32_t entry = (uint32_t) sram_entry;
    print_hex("arming entry ", entry);
    uart_flush();
    WATCHDOG_CTRL &= ~WATCHDOG_CTRL_ENABLE;
    WATCHDOG_SCRATCH(4) = BOOT_MAGIC;
    WATCHDOG_SCRATCH(5) = entry ^ -BOOT_MAGIC;
    WATCHDOG_SCRATCH(6) = SRAM_STACK_TOP;
    WATCHDOG_SCRATCH(7) = entry;
    PSM_WDSEL = PSM_WDSEL_NO_OSC;
    WATCHDOG_CTRL = WATCHDOG_CTRL_TRIGGER;
    while (1) {
    }
}
