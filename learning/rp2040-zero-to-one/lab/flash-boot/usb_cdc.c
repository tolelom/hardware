/*
 * USB CDC ACM 장치. 받은 바이트를 그대로 돌려보낸다(에코).
 * 데이터시트 4.1과 pico-examples dev_lowlevel의 구조를 따르고, 인터럽트 대신
 * main 루프에서 INTS를 폴링한다. 디버그 로그는 UART0로 나간다.
 */
#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *) (addr))
#define ALIAS_CLR 0x3000u

/* RESETS (2.14) */
#define RESETS_BASE 0x4000c000u
#define RESETS_RESET_SET REG(RESETS_BASE + 0x2000u + 0x0u)
#define RESETS_RESET_CLR REG(RESETS_BASE + 0x3000u + 0x0u)
#define RESETS_RESET_DONE REG(RESETS_BASE + 0x08u)
#define RESETS_IO_BANK0 (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)
#define RESETS_PLL_SYS (1u << 12)
#define RESETS_PLL_USB (1u << 13)
#define RESETS_TIMER (1u << 21)
#define RESETS_USBCTRL (1u << 24)
#define RESETS_UART0 (1u << 22)

/* XOSC (2.16) */
#define XOSC_BASE 0x40024000u
#define XOSC_CTRL REG(XOSC_BASE + 0x00u)
#define XOSC_STATUS REG(XOSC_BASE + 0x04u)
#define XOSC_CTRL_ENABLE (0xfabu << 12)
#define XOSC_CTRL_RANGE_1_15MHZ 0xaa0u
#define XOSC_STATUS_STABLE (1u << 31)

/* CLOCKS (2.15) */
#define CLOCKS_BASE 0x40008000u
#define CLK_REF_CTRL REG(CLOCKS_BASE + 0x30u)
#define CLK_REF_SELECTED REG(CLOCKS_BASE + 0x38u)
#define CLK_SYS_CTRL REG(CLOCKS_BASE + 0x3cu)
#define CLK_SYS_SELECTED REG(CLOCKS_BASE + 0x44u)
#define CLK_PERI_CTRL REG(CLOCKS_BASE + 0x48u)
#define CLK_USB_CTRL REG(CLOCKS_BASE + 0x54u)
#define CLK_USB_CTRL_ENABLE (1u << 11)
#define CLK_USB_AUXSRC_PLL_USB (0u << 5)
#define CLK_REF_SRC_XOSC 2u
#define CLK_SYS_SRC_REF 0u
#define CLK_SYS_SRC_AUX 1u
#define CLK_SYS_AUXSRC_PLL_SYS (0u << 5)
#define CLK_PERI_CTRL_ENABLE (1u << 11)
#define CLK_PERI_AUXSRC_XOSC (4u << 5)

/* PLL (2.18). SYS: 12MHz x 125 = 1500MHz / 6 / 2 = 125MHz,
   USB: 12MHz x 100 = 1200MHz / 5 / 5 = 48MHz (SDK 기본값, clocks.h) */
#define PLL_SYS_BASE 0x40028000u
#define PLL_USB_BASE 0x4002c000u
#define PLL_CS(base) REG((base) + 0x0u)
#define PLL_PWR_CLR(base) REG((base) + ALIAS_CLR + 0x4u)
#define PLL_FBDIV_INT(base) REG((base) + 0x8u)
#define PLL_PRIM(base) REG((base) + 0xcu)
#define PLL_CS_LOCK (1u << 31)
#define PLL_PWR_PD (1u << 0)
#define PLL_PWR_POSTDIVPD (1u << 3)
#define PLL_PWR_VCOPD (1u << 5)

/* IO_BANK0 (2.19) */
#define IO_BANK0_BASE 0x40014000u
#define GPIO_CTRL(n) REG(IO_BANK0_BASE + 0x04u + (n) * 0x8u)
#define FUNCSEL_UART 2u
#define FUNCSEL_SIO 5u

/* SIO (2.3.1) */
#define SIO_BASE 0xd0000000u
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x1cu)
#define SIO_GPIO_OE_SET REG(SIO_BASE + 0x24u)


/* UART0 (4.2) */
#define UART0_BASE 0x40034000u
#define UART0_DR REG(UART0_BASE + 0x000u)
#define UART0_FR REG(UART0_BASE + 0x018u)
#define UART0_IBRD REG(UART0_BASE + 0x024u)
#define UART0_FBRD REG(UART0_BASE + 0x028u)
#define UART0_LCR_H REG(UART0_BASE + 0x02cu)
#define UART0_CR REG(UART0_BASE + 0x030u)
#define UART_FR_TXFF (1u << 5)
#define UART_LCR_H_WLEN_8 (3u << 5)
#define UART_LCR_H_FEN (1u << 4)
#define UART_CR_UARTEN (1u << 0)
#define UART_CR_TXE (1u << 8)
#define UART_CR_RXE (1u << 9)

/* 12MHz / (16 x 115200) = 6.5104. 정수부 6, 소수부 0.5104 x 64 = 32.67 -> 33 */
#define UART_IBRD_115200 6u
#define UART_FBRD_115200 33u

#define UART_TX_PIN 0u
#define UART_RX_PIN 1u
#define LED_RED 17u

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
    }
}

static void unreset(uint32_t blocks) {
    RESETS_RESET_CLR = blocks;
    while ((RESETS_RESET_DONE & blocks) != blocks) {
    }
}

static void xosc_start(void) {
    XOSC_CTRL = XOSC_CTRL_ENABLE | XOSC_CTRL_RANGE_1_15MHZ;
    while (!(XOSC_STATUS & XOSC_STATUS_STABLE)) {
    }
}

/* AUXSRC는 바꿀 때 글리치가 나므로 끈 상태에서 바꾼다 */
static void clk_peri_from_xosc(void) {
    CLK_PERI_CTRL = 0;
    delay(10);
    CLK_PERI_CTRL = CLK_PERI_AUXSRC_XOSC;
    CLK_PERI_CTRL = CLK_PERI_AUXSRC_XOSC | CLK_PERI_CTRL_ENABLE;
}

/* IBRD, FBRD는 LCR_H를 쓸 때 반영되므로 LCR_H가 뒤에 온다 */
static void uart_init(void) {
    unreset(RESETS_UART0);
    UART0_IBRD = UART_IBRD_115200;
    UART0_FBRD = UART_FBRD_115200;
    UART0_LCR_H = UART_LCR_H_WLEN_8 | UART_LCR_H_FEN;
    UART0_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    GPIO_CTRL(UART_TX_PIN) = FUNCSEL_UART;
    GPIO_CTRL(UART_RX_PIN) = FUNCSEL_UART;
}

static void uart_putc(char c) {
    while (UART0_FR & UART_FR_TXFF) {
    }
    UART0_DR = (uint32_t) c;
}

static void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

static void led_init(void) {
    GPIO_CTRL(LED_RED) = FUNCSEL_SIO;
    SIO_GPIO_OUT_SET = 1u << LED_RED;
    SIO_GPIO_OE_SET = 1u << LED_RED;
}


static void reset_block(uint32_t blocks) {
    RESETS_RESET_SET = blocks;
    unreset(blocks);
}

/* clk_ref는 glitchless mux라 켠 채로 바꾸고 SELECTED로 완료를 기다린다 */
static void clk_ref_from_xosc(void) {
    CLK_REF_CTRL = CLK_REF_SRC_XOSC;
    while (CLK_REF_SELECTED != (1u << CLK_REF_SRC_XOSC)) {
    }
}

/* 2.18.3 순서: 분주값 → 전원·VCO 켬 → LOCK 대기 → post divider 켬 */
static void pll_start(uint32_t base, uint32_t reset_bit, uint32_t fbdiv,
                      uint32_t postdiv1, uint32_t postdiv2) {
    reset_block(reset_bit);
    PLL_CS(base) = 1u;
    PLL_FBDIV_INT(base) = fbdiv;
    PLL_PWR_CLR(base) = PLL_PWR_PD | PLL_PWR_VCOPD;
    while (!(PLL_CS(base) & PLL_CS_LOCK)) {
    }
    PLL_PRIM(base) = (postdiv1 << 16) | (postdiv2 << 12);
    PLL_PWR_CLR(base) = PLL_PWR_POSTDIVPD;
}

/* AUXSRC는 바꿀 때 글리치가 나므로 SRC를 clk_ref에 둔 상태에서 바꾼다 */
static void clk_sys_from_pll(void) {
    CLK_SYS_CTRL = CLK_SYS_SRC_REF;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_REF)) {
    }
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_REF;
    CLK_SYS_CTRL = CLK_SYS_AUXSRC_PLL_SYS | CLK_SYS_SRC_AUX;
    while (CLK_SYS_SELECTED != (1u << CLK_SYS_SRC_AUX)) {
    }
}

static void uart_puthex(uint32_t value, int digits) {
    static const char hex[] = "0123456789abcdef";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xfu]);
    }
}

/* ---------------------------------------------------------------- USB --- */

/* USB 레지스터 (4.1.4). DPRAM은 set/clear 별칭이 없다 (4.1.2.7) */
#define USB_DPRAM_BASE 0x50100000u
#define USB_REGS_BASE 0x50110000u
#define USB_ADDR_ENDP REG(USB_REGS_BASE + 0x00u)
#define USB_MAIN_CTRL REG(USB_REGS_BASE + 0x40u)
#define USB_SIE_CTRL REG(USB_REGS_BASE + 0x4cu)
#define USB_SIE_STATUS REG(USB_REGS_BASE + 0x50u)
#define USB_BUFF_STATUS REG(USB_REGS_BASE + 0x58u)
#define USB_EP_STALL_ARM REG(USB_REGS_BASE + 0x68u)
#define USB_MUXING REG(USB_REGS_BASE + 0x74u)
#define USB_PWR REG(USB_REGS_BASE + 0x78u)
#define USB_INTE REG(USB_REGS_BASE + 0x90u)
#define USB_INTS REG(USB_REGS_BASE + 0x98u)

#define MAIN_CTRL_CONTROLLER_EN (1u << 0)
#define SIE_CTRL_PULLUP_EN (1u << 16)
#define SIE_CTRL_EP0_INT_1BUF (1u << 29)
#define SIE_STATUS_SETUP_REC (1u << 17)
#define SIE_STATUS_BUS_RESET (1u << 19)
#define MUXING_TO_PHY (1u << 0)
#define MUXING_SOFTCON (1u << 3)
#define PWR_VBUS_DETECT (1u << 2)
#define PWR_VBUS_DETECT_OVERRIDE_EN (1u << 3)
#define INTS_BUFF_STATUS (1u << 4)
#define INTS_BUS_RESET (1u << 12)
#define INTS_SETUP_REQ (1u << 16)

/* DPRAM 배치 (Table 394). 버퍼는 64바이트 정렬 */
#define DPRAM8(offset) ((volatile uint8_t *) (USB_DPRAM_BASE + (offset)))
#define DPRAM32(offset) REG(USB_DPRAM_BASE + (offset))
#define DPRAM_SIZE 0x1000u
#define EP1_IN_CTRL 0x08u
#define EP2_IN_CTRL 0x10u
#define EP2_OUT_CTRL 0x14u
#define EP0_IN_BUF_CTRL 0x80u
#define EP0_OUT_BUF_CTRL 0x84u
#define EP2_IN_BUF_CTRL 0x90u
#define EP2_OUT_BUF_CTRL 0x94u
#define EP0_BUF 0x100u
#define EP1_IN_BUF 0x180u
#define EP2_OUT_BUF 0x1c0u
#define EP2_IN_BUF 0x200u

/* 엔드포인트 제어 (Table 395) */
#define EP_CTRL_ENABLE (1u << 31)
#define EP_CTRL_INT_PER_BUFFER (1u << 29)
#define EP_CTRL_TYPE_BULK (2u << 26)
#define EP_CTRL_TYPE_INTERRUPT (3u << 26)

/* 버퍼 제어 (Table 396) */
#define BUF_FULL (1u << 15)
#define BUF_LAST (1u << 14)
#define BUF_DATA1 (1u << 13)
#define BUF_STALL (1u << 11)
#define BUF_AVAILABLE (1u << 10)
#define BUF_LEN_MASK 0x3ffu

/* BUFF_STATUS: bit 2n = EPn IN, 2n+1 = EPn OUT */
#define BUFF_EP0_IN (1u << 0)
#define BUFF_EP0_OUT (1u << 1)
#define BUFF_EP2_IN (1u << 4)
#define BUFF_EP2_OUT (1u << 5)

#define EP0_SIZE 64u
#define BULK_SIZE 64u

/* USB 2.0 9장, CDC 1.2 PSTN */
#define REQ_GET_STATUS 0x00u
#define REQ_SET_ADDRESS 0x05u
#define REQ_GET_DESCRIPTOR 0x06u
#define REQ_GET_CONFIGURATION 0x08u
#define REQ_SET_CONFIGURATION 0x09u
#define REQ_SET_LINE_CODING 0x20u
#define REQ_GET_LINE_CODING 0x21u
#define REQ_SET_CONTROL_LINE_STATE 0x22u
#define DESC_DEVICE 1u
#define DESC_CONFIG 2u
#define DESC_STRING 3u
#define REQ_TYPE_STANDARD 0u
#define REQ_TYPE_CLASS 1u
#define REQ_DIR_IN 0x80u

/* VID/PID 1209:0001은 pid.codes가 시험용으로 열어 둔 번호 */
static const uint8_t device_descriptor[] = {
    18, DESC_DEVICE, 0x00, 0x02,  /* USB 2.0 */
    0x02, 0x00, 0x00,             /* class CDC */
    EP0_SIZE,
    0x09, 0x12, 0x01, 0x00,       /* VID 0x1209, PID 0x0001 */
    0x00, 0x01,                   /* bcdDevice 1.00 */
    1, 2, 3,                      /* 제조사, 제품, 일련번호 문자열 */
    1,
};

static const uint8_t config_descriptor[] = {
    9, DESC_CONFIG, 67, 0, 2, 1, 0, 0x80, 50,
    /* 인터페이스 0: CDC 통신 (ACM) */
    9, 4, 0, 0, 1, 0x02, 0x02, 0x00, 0,
    5, 0x24, 0x00, 0x10, 0x01,    /* header, CDC 1.10 */
    5, 0x24, 0x01, 0x00, 0x01,    /* call management */
    4, 0x24, 0x02, 0x02,          /* ACM: line coding 지원 */
    5, 0x24, 0x06, 0x00, 0x01,    /* union: 0 = master, 1 = slave */
    7, 5, 0x81, 0x03, 8, 0, 16,   /* EP1 IN interrupt */
    /* 인터페이스 1: CDC 데이터 */
    9, 4, 1, 0, 2, 0x0a, 0x00, 0x00, 0,
    7, 5, 0x02, 0x02, BULK_SIZE, 0, 0,  /* EP2 OUT bulk */
    7, 5, 0x82, 0x02, BULK_SIZE, 0, 0,  /* EP2 IN bulk */
};

static const char *const strings[] = {
    0, "tolelom", "RP2040 0to1 CDC", "0001",
};
#define STRING_COUNT 4u

/* 115200 8N1. 호스트가 바꿔도 기록만 한다 */
static uint8_t line_coding[7] = {0x00, 0xc2, 0x01, 0x00, 0, 0, 8};
static uint8_t string_buffer[2 + 2 * 32];

static uint32_t ep0_in_pid;
static uint32_t ep0_out_pid;
static uint32_t ep2_in_pid;
static uint32_t ep2_out_pid;
static uint32_t ep2_in_last_len;
static const uint8_t *ep0_tx_data;
static uint32_t ep0_tx_left;
static int ep0_tx_zlp;
static int ep0_in_data_stage;
static int ep0_expect_line_coding;
static uint32_t pending_address;
static uint32_t configured;
static int reboot_to_bootsel;

/* bootrom 함수 표 (2.8.3). 0x14에 표, 0x18에 조회 함수의 16비트 주소 */
#define ROM_FUNC_TABLE_PTR 0x00000014u
#define ROM_TABLE_LOOKUP_PTR 0x00000018u
#define ROM_CODE(c1, c2) ((uint32_t) (c1) | ((uint32_t) (c2) << 8))
#define BOOTSEL_TOUCH_BAUD 1200u
typedef void *(*rom_table_lookup_fn)(uint16_t *table, uint32_t code);
typedef void (*rom_reset_to_usb_boot_fn)(uint32_t gpio_activity_pin_mask,
                                         uint32_t disable_interface_mask);

static void *rom_func_lookup(uint32_t code) {
    uint16_t *table = (uint16_t *) (uintptr_t) *(volatile uint16_t *) ROM_FUNC_TABLE_PTR;
    rom_table_lookup_fn lookup =
        (rom_table_lookup_fn) (uintptr_t) *(volatile uint16_t *) ROM_TABLE_LOOKUP_PTR;
    return lookup(table, code);
}

/* watchdog으로 리셋해 BOOTSEL 모드로 들어간다. 돌아오지 않는다 */
static void reset_to_bootsel(void) {
    uart_puts("reboot to BOOTSEL\r\n");
    while (!(UART0_FR & (1u << 7))) {  /* TXFE: 로그를 다 내보낸 뒤 */
    }
    rom_reset_to_usb_boot_fn reset =
        (rom_reset_to_usb_boot_fn) rom_func_lookup(ROM_CODE('U', 'B'));
    reset(0u, 0u);
}

/* clk_sys와 clk_usb가 다르면 AVAILABLE을 나머지 비트보다 늦게 써야 한다 (4.1.2.7.4) */
static void buf_ctrl_write(uint32_t offset, uint32_t value) {
    DPRAM32(offset) = value & ~BUF_AVAILABLE;
    for (volatile int i = 0; i < 4; i++) {
    }
    DPRAM32(offset) = value;
}

static void ep0_in_send_chunk(void) {
    uint32_t len = ep0_tx_left < EP0_SIZE ? ep0_tx_left : EP0_SIZE;
    for (uint32_t i = 0; i < len; i++) {
        DPRAM8(EP0_BUF)[i] = ep0_tx_data[i];
    }
    ep0_tx_data += len;
    ep0_tx_left -= len;
    buf_ctrl_write(EP0_IN_BUF_CTRL, len | BUF_FULL | BUF_AVAILABLE
                   | (ep0_in_pid ? BUF_DATA1 : 0u));
    ep0_in_pid ^= 1u;
}

/* 데이터 단계: 요청 길이로 자르고, 64의 배수로 끝나면 길이 0 패킷을 붙인다 */
static void ep0_in_start(const uint8_t *data, uint32_t len, uint32_t requested) {
    if (len > requested) {
        len = requested;
    }
    ep0_tx_data = data;
    ep0_tx_left = len;
    ep0_tx_zlp = (len < requested) && (len % EP0_SIZE == 0u);
    ep0_in_data_stage = 1;
    ep0_in_send_chunk();
}

/* 상태 단계: 데이터 없는 OUT 요청에는 길이 0 IN 패킷으로 답한다 */
static void ep0_in_status(void) {
    ep0_in_data_stage = 0;
    buf_ctrl_write(EP0_IN_BUF_CTRL, BUF_FULL | BUF_AVAILABLE | BUF_DATA1);
}

static void ep0_out_receive(void) {
    buf_ctrl_write(EP0_OUT_BUF_CTRL, EP0_SIZE | BUF_AVAILABLE
                   | (ep0_out_pid ? BUF_DATA1 : 0u));
    ep0_out_pid ^= 1u;
}

/* EP0 STALL은 EP_STALL_ARM과 버퍼 제어의 STALL을 함께 세운다 (4.1.2.8.1) */
static void ep0_stall(void) {
    USB_EP_STALL_ARM = 0x3u;
    DPRAM32(EP0_IN_BUF_CTRL) = BUF_STALL;
    DPRAM32(EP0_OUT_BUF_CTRL) = BUF_STALL;
}

static void ep2_out_receive(void) {
    buf_ctrl_write(EP2_OUT_BUF_CTRL, BULK_SIZE | BUF_AVAILABLE
                   | (ep2_out_pid ? BUF_DATA1 : 0u));
    ep2_out_pid ^= 1u;
}

static uint32_t string_descriptor(uint32_t index) {
    if (index == 0u) {
        string_buffer[0] = 4;
        string_buffer[1] = DESC_STRING;
        string_buffer[2] = 0x09;  /* 0x0409 영어(미국) */
        string_buffer[3] = 0x04;
        return 4u;
    }
    const char *s = strings[index];
    uint32_t n = 0;
    while (s[n] && n < 32u) {
        string_buffer[2 + 2 * n] = (uint8_t) s[n];
        string_buffer[3 + 2 * n] = 0;
        n++;
    }
    string_buffer[0] = (uint8_t) (2u + 2u * n);
    string_buffer[1] = DESC_STRING;
    return 2u + 2u * n;
}

static void handle_get_descriptor(uint32_t value, uint32_t length) {
    uint32_t type = value >> 8;
    uint32_t index = value & 0xffu;
    if (type == DESC_DEVICE) {
        ep0_in_start(device_descriptor, sizeof device_descriptor, length);
    } else if (type == DESC_CONFIG) {
        ep0_in_start(config_descriptor, sizeof config_descriptor, length);
    } else if (type == DESC_STRING && index < STRING_COUNT) {
        ep0_in_start(string_buffer, string_descriptor(index), length);
    } else {
        ep0_stall();  /* device qualifier 등. full speed 장치는 STALL이 정답 */
    }
}

static const uint8_t zero_status[2] = {0, 0};
static uint8_t configuration_value;

static void handle_setup(void) {
    volatile uint8_t *p = DPRAM8(0);
    uint32_t request_type = p[0];
    uint32_t request = p[1];
    uint32_t value = p[2] | ((uint32_t) p[3] << 8);
    uint32_t length = p[6] | ((uint32_t) p[7] << 8);
    uint32_t type = (request_type >> 5) & 0x3u;
    int dir_in = (request_type & REQ_DIR_IN) != 0;

    /* SETUP 다음 데이터·상태 단계는 항상 DATA1부터 */
    ep0_in_pid = 1u;
    ep0_out_pid = 1u;
    ep0_expect_line_coding = 0;

    if (type == REQ_TYPE_STANDARD && request == REQ_GET_DESCRIPTOR) {
        handle_get_descriptor(value, length);
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_ADDRESS) {
        pending_address = value & 0x7fu;
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_CONFIGURATION) {
        configured = value & 0xffu;
        configuration_value = (uint8_t) configured;
        ep2_in_pid = 0u;
        ep2_out_pid = 0u;
        ep2_out_receive();
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_CONFIGURATION) {
        ep0_in_start(&configuration_value, 1u, length);
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_STATUS) {
        ep0_in_start(zero_status, 2u, length);
    } else if (type == REQ_TYPE_CLASS && request == REQ_SET_LINE_CODING) {
        ep0_expect_line_coding = 1;
        ep0_out_receive();
    } else if (type == REQ_TYPE_CLASS && request == REQ_GET_LINE_CODING) {
        ep0_in_start(line_coding, sizeof line_coding, length);
    } else if (!dir_in && length == 0u) {
        ep0_in_status();  /* SET_CONTROL_LINE_STATE 등 데이터 없는 요청 */
    } else {
        ep0_stall();
    }

    uart_puts("S ");
    uart_puthex(request_type, 2);
    uart_putc(' ');
    uart_puthex(request, 2);
    uart_putc(' ');
    uart_puthex(value, 4);
    uart_putc(' ');
    uart_puthex(length, 4);
    uart_puts("\r\n");
}

static void handle_ep0_in_done(void) {
    if (ep0_tx_left > 0u || ep0_tx_zlp) {
        if (ep0_tx_left == 0u) {
            ep0_tx_zlp = 0;
        }
        ep0_in_send_chunk();
        return;
    }
    if (ep0_in_data_stage) {
        ep0_in_data_stage = 0;
        ep0_out_pid = 1u;
        ep0_out_receive();  /* 호스트의 길이 0 OUT 상태 패킷을 받을 자리 */
        return;
    }
    if (reboot_to_bootsel) {
        reset_to_bootsel();  /* SET_LINE_CODING 상태 단계가 끝난 뒤 */
    }
    if (pending_address) {
        USB_ADDR_ENDP = pending_address;  /* 상태 단계가 끝난 뒤에 주소를 바꾼다 */
        uart_puts("addr ");
        uart_puthex(pending_address, 2);
        uart_puts("\r\n");
        pending_address = 0u;
    }
}

static void handle_ep0_out_done(void) {
    if (!ep0_expect_line_coding) {
        return;
    }
    ep0_expect_line_coding = 0;
    for (uint32_t i = 0; i < sizeof line_coding; i++) {
        line_coding[i] = DPRAM8(EP0_BUF)[i];
    }
    /* 1200 baud 설정을 BOOTSEL 재부팅 요청으로 쓴다 (SDK, Arduino 관례) */
    uint32_t baud = line_coding[0] | ((uint32_t) line_coding[1] << 8)
                    | ((uint32_t) line_coding[2] << 16)
                    | ((uint32_t) line_coding[3] << 24);
    reboot_to_bootsel = (baud == BOOTSEL_TOUCH_BAUD);
    ep0_in_status();
}

static void ep2_in_send(uint32_t len) {
    ep2_in_last_len = len;
    buf_ctrl_write(EP2_IN_BUF_CTRL, len | BUF_FULL | BUF_AVAILABLE
                   | (ep2_in_pid ? BUF_DATA1 : 0u));
    ep2_in_pid ^= 1u;
}

/* 받은 패킷을 그대로 EP2 IN으로 돌려보낸다. 보내기가 끝나야 다음을 받는다 */
static void handle_ep2_out_done(void) {
    uint32_t len = DPRAM32(EP2_OUT_BUF_CTRL) & BUF_LEN_MASK;
    for (uint32_t i = 0; i < len; i++) {
        DPRAM8(EP2_IN_BUF)[i] = DPRAM8(EP2_OUT_BUF)[i];
    }
    ep2_in_send(len);
    SIO_GPIO_OUT_XOR = 1u << LED_RED;
}

/* 꽉 찬 패킷(64바이트)으로 끝나면 호스트는 전송이 이어진다고 본다.
   길이 0 패킷을 보내 끝을 알린 뒤 다음 OUT을 받는다 */
static void handle_ep2_in_done(void) {
    if (ep2_in_last_len == BULK_SIZE) {
        ep2_in_send(0u);
        return;
    }
    ep2_out_receive();
}

static void handle_bus_reset(void) {
    USB_ADDR_ENDP = 0u;
    pending_address = 0u;
    configured = 0u;
    ep0_tx_left = 0u;
    ep0_tx_zlp = 0;
    ep0_in_data_stage = 0;
    uart_puts("bus reset\r\n");
}

static void usb_init(void) {
    reset_block(RESETS_USBCTRL);
    for (uint32_t offset = 0; offset < DPRAM_SIZE; offset += 4u) {
        DPRAM32(offset) = 0u;
    }
    USB_MUXING = MUXING_TO_PHY | MUXING_SOFTCON;
    /* VBUS 감지 핀이 없어 VBUS가 있다고 강제한다 */
    USB_PWR = PWR_VBUS_DETECT | PWR_VBUS_DETECT_OVERRIDE_EN;
    USB_MAIN_CTRL = MAIN_CTRL_CONTROLLER_EN;
    USB_SIE_CTRL = SIE_CTRL_EP0_INT_1BUF;
    USB_INTE = INTS_BUFF_STATUS | INTS_BUS_RESET | INTS_SETUP_REQ;
    DPRAM32(EP1_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                           | EP_CTRL_TYPE_INTERRUPT | EP1_IN_BUF;
    DPRAM32(EP2_OUT_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                            | EP_CTRL_TYPE_BULK | EP2_OUT_BUF;
    DPRAM32(EP2_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                           | EP_CTRL_TYPE_BULK | EP2_IN_BUF;
    /* D+ 풀업을 켜면 호스트가 full speed 장치가 붙었다고 본다 */
    USB_SIE_CTRL = SIE_CTRL_EP0_INT_1BUF | SIE_CTRL_PULLUP_EN;
}

static void usb_poll(void) {
    uint32_t status = USB_INTS;
    if (status & INTS_SETUP_REQ) {
        USB_SIE_STATUS = SIE_STATUS_SETUP_REC;
        handle_setup();
    }
    if (status & INTS_BUFF_STATUS) {
        uint32_t buffers = USB_BUFF_STATUS;
        USB_BUFF_STATUS = buffers;
        if (buffers & BUFF_EP0_IN) {
            handle_ep0_in_done();
        }
        if (buffers & BUFF_EP0_OUT) {
            handle_ep0_out_done();
        }
        if (buffers & BUFF_EP2_OUT) {
            handle_ep2_out_done();
        }
        if (buffers & BUFF_EP2_IN) {
            handle_ep2_in_done();
        }
    }
    if (status & INTS_BUS_RESET) {
        USB_SIE_STATUS = SIE_STATUS_BUS_RESET;
        handle_bus_reset();
    }
}

/* E16: USB를 쓰려면 clk_sys가 48MHz보다 빨라야 한다 (4.1.2.1) */
static void clocks_init(void) {
    xosc_start();
    clk_peri_from_xosc();
    clk_ref_from_xosc();
    pll_start(PLL_SYS_BASE, RESETS_PLL_SYS, 125u, 6u, 2u);
    clk_sys_from_pll();
    pll_start(PLL_USB_BASE, RESETS_PLL_USB, 100u, 5u, 5u);
    CLK_USB_CTRL = 0u;
    delay(10);
    CLK_USB_CTRL = CLK_USB_AUXSRC_PLL_USB;
    CLK_USB_CTRL = CLK_USB_AUXSRC_PLL_USB | CLK_USB_CTRL_ENABLE;
}

int main(void) {
    unreset(RESETS_IO_BANK0 | RESETS_PADS_BANK0);
    led_init();
    clocks_init();
    uart_init();
    uart_puts("usb cdc start\r\n");
    usb_init();
    while (1) {
        usb_poll();
    }
}
