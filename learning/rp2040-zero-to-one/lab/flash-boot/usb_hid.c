/*
 * USB HID boot 키보드 (HID 1.11, 데이터시트 4.1). 키는 BOOT 버튼 하나이고 Caps Lock을 보낸다.
 * BOOT 버튼은 flash CS(QSPI_SS)에 붙어 있어, 읽는 동안 CS 출력을 끄므로 XIP를 쓸 수 없다.
 * 읽기 함수는 SRAM에 두고 인터럽트를 끈 채 부른다 (pico-examples picoboard/button과 같은 방식).
 *
 * 호스트가 보내는 LED 상태(SET_REPORT, 출력 리포트 1바이트)를 받아 RGB LED에 보인다.
 *   초록 = Num Lock, 파랑 = Caps Lock
 * UART로 'N'을 받으면 Num Lock을 한 번 눌렀다 뗀다. 호스트가 LED 상태를 바꿔 돌려보내면
 * 키 전송과 LED 수신이 모두 된 것이다. 글자는 입력되지 않는다.
 * USB 장치 쪽 처리(EP0, 버스 리셋)는 usb_msc.c와 같다.
 */
#include "board.h"

/* ------------------------------------------------------------ USB 레지스터 --- */
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

#define DPRAM8(offset) ((volatile uint8_t *) (USB_DPRAM_BASE + (offset)))
#define DPRAM32(offset) REG(USB_DPRAM_BASE + (offset))
#define DPRAM_SIZE 0x1000u
#define EP1_IN_CTRL 0x08u
#define EP0_IN_BUF_CTRL 0x80u
#define EP0_OUT_BUF_CTRL 0x84u
#define EP1_IN_BUF_CTRL 0x88u
#define EP0_BUF 0x100u
#define EP1_IN_BUF 0x180u

#define EP_CTRL_ENABLE (1u << 31)
#define EP_CTRL_INT_PER_BUFFER (1u << 29)
#define EP_CTRL_TYPE_INTERRUPT (3u << 26)

#define BUF_FULL (1u << 15)
#define BUF_DATA1 (1u << 13)
#define BUF_STALL (1u << 11)
#define BUF_AVAILABLE (1u << 10)
#define BUF_LEN_MASK 0x3ffu

#define BUFF_EP0_IN (1u << 0)
#define BUFF_EP0_OUT (1u << 1)
#define BUFF_EP1_IN (1u << 2)

#define EP0_SIZE 64u
#define REPORT_SIZE 8u

#define REQ_GET_STATUS 0x00u
#define REQ_SET_ADDRESS 0x05u
#define REQ_GET_DESCRIPTOR 0x06u
#define REQ_GET_CONFIGURATION 0x08u
#define REQ_SET_CONFIGURATION 0x09u
#define REQ_HID_GET_REPORT 0x01u
#define REQ_HID_GET_IDLE 0x02u
#define REQ_HID_GET_PROTOCOL 0x03u
#define REQ_HID_SET_REPORT 0x09u
#define REQ_HID_SET_IDLE 0x0au
#define REQ_HID_SET_PROTOCOL 0x0bu
#define DESC_DEVICE 1u
#define DESC_CONFIG 2u
#define DESC_STRING 3u
#define DESC_HID 0x21u
#define DESC_HID_REPORT 0x22u
#define REQ_TYPE_STANDARD 0u
#define REQ_TYPE_CLASS 1u
#define REQ_DIR_IN 0x80u

/* HID 사용 페이지 0x07 키 코드 (HID Usage Tables 10절) */
#define KEY_CAPS_LOCK 0x39u
#define KEY_NUM_LOCK 0x53u
#define LED_BIT_NUM (1u << 0)
#define LED_BIT_CAPS (1u << 1)

static const uint8_t device_descriptor[] = {
    18, DESC_DEVICE, 0x00, 0x02,
    0x00, 0x00, 0x00,             /* class는 인터페이스에서 정한다 */
    EP0_SIZE,
    0x09, 0x12, 0x01, 0x00,       /* VID 0x1209, PID 0x0001 (pid.codes 시험용) */
    0x00, 0x03,                   /* bcdDevice 3.00. CDC, MSC 펌웨어와 구분 */
    1, 2, 3,
    1,
};

/* HID 1.11 부록 B.1 boot 키보드 리포트: 수식 키 8비트, 예약 1바이트, 키 6개, LED 출력 5비트 */
static const uint8_t report_descriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,       /* Generic Desktop, Keyboard, Collection */
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,       /* 수식 키 E0~E7 */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,       /* 예약 바이트 */
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, /* LED 5개 */
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,       /* LED 채움 3비트 */
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, /* 키 배열 6개 */
    0xc0,
};

#define HID_DESC_OFFSET 18u
static const uint8_t config_descriptor[] = {
    9, DESC_CONFIG, 34, 0, 1, 1, 0, 0x80, 50,
    /* 인터페이스 0: HID, boot subclass 1, 키보드 protocol 1 */
    9, 4, 0, 0, 1, 0x03, 0x01, 0x01, 0,
    /* HID 1.11, 나라 코드 0, 디스크립터 1개(report) */
    9, DESC_HID, 0x11, 0x01, 0, 1, DESC_HID_REPORT, sizeof report_descriptor, 0,
    7, 5, 0x81, 0x03, REPORT_SIZE, 0, 1,  /* EP1 IN interrupt, 1ms */
};

static const char *const strings[] = {
    0, "tolelom", "RP2040 0to1 HID", "000000000004",
};
#define STRING_COUNT 4u

static uint8_t string_buffer[2 + 2 * 32];
static uint32_t ep0_in_pid;
static uint32_t ep0_out_pid;
static uint32_t ep1_in_pid;
static const uint8_t *ep0_tx_data;
static uint32_t ep0_tx_left;
static int ep0_tx_zlp;
static int ep0_in_data_stage;
static int ep0_out_set_report;
static uint32_t pending_address;
static uint8_t configuration_value;
static uint8_t protocol = 1u;      /* SET_PROTOCOL 전 기본값은 report 프로토콜(HID 7.2.6) */
static uint8_t idle_rate;
static const uint8_t zero_bytes[2] = {0, 0};

static uint8_t report[REPORT_SIZE];
static int ep1_busy;
static uint8_t leds = 0xffu;       /* 아직 받지 않음 */
static uint32_t led_reports;
static uint32_t numlock_sent_us;
static int numlock_release_pending;
static uint32_t ep1_reports_sent;

static void buf_ctrl_write(uint32_t offset, uint32_t value) {
    DPRAM32(offset) = value & ~BUF_AVAILABLE;
    for (volatile int i = 0; i < 4; i++) {
    }
    DPRAM32(offset) = value;
}

/* ------------------------------------------------------------ BOOT 버튼 --- */
#define IO_QSPI_SS_CTRL REG(0x40018000u + 0x0cu)
#define QSPI_OEOVER_MASK (3u << 12)
#define QSPI_OEOVER_DISABLE (2u << 12)
#define SIO_GPIO_HI_IN REG(SIO_BASE + 0x008u)
#define SIO_GPIO_HI_IN_QSPI_SS (1u << 1)

#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

/* CS 출력을 끄고 핀이 버튼으로 내려가 있는지 본다. 이 동안 flash를 읽으면 안 되므로
   본문, 대기 루프, 레지스터 주소 모두 SRAM과 레지스터 안에 있다. 인터럽트는 부르는 쪽이 끈다 */
RAMFUNC static int boot_button_pressed(void) {
    uint32_t ctrl = IO_QSPI_SS_CTRL;
    IO_QSPI_SS_CTRL = (ctrl & ~QSPI_OEOVER_MASK) | QSPI_OEOVER_DISABLE;
    for (volatile int i = 0; i < 1000; i++) {
    }
    int pressed = !(SIO_GPIO_HI_IN & SIO_GPIO_HI_IN_QSPI_SS);
    IO_QSPI_SS_CTRL = ctrl;
    return pressed;
}

static int read_boot_button(void) {
    __asm volatile("cpsid i");
    int pressed = boot_button_pressed();
    __asm volatile("cpsie i");
    return pressed;
}

/* ------------------------------------------------------------ 키보드 리포트 --- */
static void ep1_send_report(void) {
    for (uint32_t i = 0; i < REPORT_SIZE; i++) {
        DPRAM8(EP1_IN_BUF)[i] = report[i];
    }
    buf_ctrl_write(EP1_IN_BUF_CTRL, REPORT_SIZE | BUF_FULL | BUF_AVAILABLE
                   | (ep1_in_pid ? BUF_DATA1 : 0u));
    ep1_in_pid ^= 1u;
    ep1_busy = 1;
    ep1_reports_sent++;
}

/* 키 하나만 누르거나 모두 뗀 리포트를 보낸다. 키 코드는 위 두 상수만 쓴다 */
static void set_key(uint32_t key) {
    for (uint32_t i = 0; i < REPORT_SIZE; i++) {
        report[i] = 0;
    }
    report[2] = (uint8_t) key;
    ep1_send_report();
}

static void show_leds(void) {
    if (leds & LED_BIT_NUM) {
        SIO_GPIO_OUT_CLR = 1u << LED_GREEN;
    } else {
        SIO_GPIO_OUT_SET = 1u << LED_GREEN;
    }
    if (leds & LED_BIT_CAPS) {
        SIO_GPIO_OUT_CLR = 1u << LED_BLUE;
    } else {
        SIO_GPIO_OUT_SET = 1u << LED_BLUE;
    }
}

static void handle_led_report(uint32_t len) {
    uint32_t now = time_us();
    uint8_t value = DPRAM8(EP0_BUF)[0];
    led_reports++;
    uart_puts("LED ");
    uart_puthex(value, 2);
    uart_puts(" len ");
    uart_putdec(len);
    if (numlock_sent_us) {
        uart_puts(", ");
        uart_putdec(now - numlock_sent_us);
        uart_puts(" us after Num Lock press");
        numlock_sent_us = 0;
    }
    uart_puts("\r\n");
    leds = value;
    show_leds();
}

/* ------------------------------------------------------------ EP0 --- */
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

static void ep0_in_status(void) {
    ep0_in_data_stage = 0;
    buf_ctrl_write(EP0_IN_BUF_CTRL, BUF_FULL | BUF_AVAILABLE | BUF_DATA1);
}

static void ep0_out_receive(void) {
    buf_ctrl_write(EP0_OUT_BUF_CTRL, EP0_SIZE | BUF_AVAILABLE
                   | (ep0_out_pid ? BUF_DATA1 : 0u));
    ep0_out_pid ^= 1u;
}

static void ep0_stall(void) {
    USB_EP_STALL_ARM = 0x3u;
    DPRAM32(EP0_IN_BUF_CTRL) = BUF_STALL;
    DPRAM32(EP0_OUT_BUF_CTRL) = BUF_STALL;
}

static uint32_t string_descriptor(uint32_t index) {
    if (index == 0u) {
        string_buffer[0] = 4;
        string_buffer[1] = DESC_STRING;
        string_buffer[2] = 0x09;
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

static void handle_setup(void) {
    volatile uint8_t *p = DPRAM8(0);
    uint32_t request_type = p[0];
    uint32_t request = p[1];
    uint32_t value = p[2] | ((uint32_t) p[3] << 8);
    uint32_t length = p[6] | ((uint32_t) p[7] << 8);
    uint32_t type = (request_type >> 5) & 0x3u;
    int dir_in = (request_type & REQ_DIR_IN) != 0;

    ep0_in_pid = 1u;
    ep0_out_pid = 1u;
    ep0_out_set_report = 0;

    if (type == REQ_TYPE_STANDARD && request == REQ_GET_DESCRIPTOR) {
        uint32_t desc = value >> 8;
        uint32_t desc_index = value & 0xffu;
        if (desc == DESC_DEVICE) {
            ep0_in_start(device_descriptor, sizeof device_descriptor, length);
        } else if (desc == DESC_CONFIG) {
            ep0_in_start(config_descriptor, sizeof config_descriptor, length);
        } else if (desc == DESC_STRING && desc_index < STRING_COUNT) {
            ep0_in_start(string_buffer, string_descriptor(desc_index), length);
        } else if (desc == DESC_HID) {
            ep0_in_start(&config_descriptor[HID_DESC_OFFSET], 9u, length);
        } else if (desc == DESC_HID_REPORT) {
            ep0_in_start(report_descriptor, sizeof report_descriptor, length);
        } else {
            ep0_stall();
        }
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_ADDRESS) {
        pending_address = value & 0x7fu;
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_CONFIGURATION) {
        configuration_value = (uint8_t) value;
        ep1_in_pid = 0u;
        ep1_busy = 0;
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_CONFIGURATION) {
        ep0_in_start(&configuration_value, 1u, length);
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_STATUS) {
        ep0_in_start(zero_bytes, 2u, length);
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_SET_REPORT && !dir_in
               && length > 0u && length <= EP0_SIZE) {
        /* 데이터 단계(OUT)로 LED 바이트가 온다. 받은 뒤 상태 단계 IN을 보낸다 */
        ep0_out_set_report = 1;
        ep0_out_receive();
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_GET_REPORT) {
        ep0_in_start(report, REPORT_SIZE, length);
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_SET_IDLE) {
        idle_rate = (uint8_t) (value >> 8);
        ep0_in_status();
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_GET_IDLE) {
        ep0_in_start(&idle_rate, 1u, length);
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_SET_PROTOCOL) {
        protocol = (uint8_t) value;
        ep0_in_status();
    } else if (type == REQ_TYPE_CLASS && request == REQ_HID_GET_PROTOCOL) {
        ep0_in_start(&protocol, 1u, length);
    } else if (!dir_in && length == 0u) {
        ep0_in_status();
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
        ep0_out_receive();
        return;
    }
    if (pending_address) {
        USB_ADDR_ENDP = pending_address;
        pending_address = 0u;
    }
}

static void handle_ep0_out_done(void) {
    if (ep0_out_set_report) {
        ep0_out_set_report = 0;
        handle_led_report(DPRAM32(EP0_OUT_BUF_CTRL) & BUF_LEN_MASK);
        ep0_in_status();
    }
    /* 그 밖에는 IN 데이터 단계 뒤 상태 단계의 길이 0 OUT */
}

static void handle_bus_reset(void) {
    USB_ADDR_ENDP = 0u;
    pending_address = 0u;
    ep0_tx_left = 0u;
    ep0_tx_zlp = 0;
    ep0_in_data_stage = 0;
    ep0_out_set_report = 0;
    configuration_value = 0;
    uart_puts("bus reset\r\n");
}

static void usb_init(void) {
    reset_block(RESETS_USBCTRL);
    for (uint32_t offset = 0; offset < DPRAM_SIZE; offset += 4u) {
        DPRAM32(offset) = 0u;
    }
    USB_MUXING = MUXING_TO_PHY | MUXING_SOFTCON;
    USB_PWR = PWR_VBUS_DETECT | PWR_VBUS_DETECT_OVERRIDE_EN;
    USB_MAIN_CTRL = MAIN_CTRL_CONTROLLER_EN;
    USB_SIE_CTRL = SIE_CTRL_EP0_INT_1BUF;
    USB_INTE = INTS_BUFF_STATUS | INTS_BUS_RESET | INTS_SETUP_REQ;
    DPRAM32(EP1_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                           | EP_CTRL_TYPE_INTERRUPT | EP1_IN_BUF;
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
        if (buffers & BUFF_EP1_IN) {
            ep1_busy = 0;
        }
    }
    if (status & INTS_BUS_RESET) {
        USB_SIE_STATUS = SIE_STATUS_BUS_RESET;
        handle_bus_reset();
    }
}

/* ------------------------------------------------------------ UART 명령 --- */
/* board.c의 UART IRQ 대신 여기서 읽는다. "BOOTSEL"은 bootrom으로, 'N'은 Num Lock 한 번 */
static int uart_poll_command(void) {
    static const char magic[] = "BOOTSEL";
    static unsigned matched;
    int numlock = 0;
    while (!(UART0_FR & UART_FR_RXFE)) {
        char c = (char) (UART0_DR & 0xffu);
        if (c == magic[matched]) {
            matched++;
            if (magic[matched] == '\0') {
                reset_to_bootsel();
            }
        } else {
            matched = (c == magic[0]) ? 1u : 0u;
        }
        if (c == 'N') {
            numlock = 1;
        }
    }
    return numlock;
}

#define IRQ_UART0 20u
#define BUTTON_POLL_US 5000u
#define BUTTON_STABLE_READS 4u  /* 5ms 간격으로 같은 값 4번 = 20ms 디바운스 */

int main(void) {
    board_init();
    NVIC_ICER = 1u << IRQ_UART0;
    clocks_48mhz_init();
    uart_puts("usb hid start\r\n");
    usb_init();

    int button = 0;
    int last_read = 0;
    uint32_t same_reads = 0;
    uint32_t last_poll = time_us();
    int numlock_queued = 0;
    while (1) {
        usb_poll();
        if (uart_poll_command()) {
            numlock_queued = 1;
        }
        if (!configuration_value || ep1_busy) {
            continue;
        }
        if (numlock_release_pending) {
            numlock_release_pending = 0;
            set_key(0);
        } else if (numlock_queued) {
            numlock_queued = 0;
            numlock_sent_us = time_us();
            set_key(KEY_NUM_LOCK);
            numlock_release_pending = 1;
            uart_puts("Num Lock press sent\r\n");
        } else if (time_us() - last_poll >= BUTTON_POLL_US) {
            last_poll = time_us();
            int now = read_boot_button();
            same_reads = (now == last_read) ? same_reads + 1u : 0u;
            last_read = now;
            if (same_reads >= BUTTON_STABLE_READS && now != button) {
                button = now;
                set_key(button ? KEY_CAPS_LOCK : 0u);
                uart_puts(button ? "BOOT down, Caps Lock press\r\n"
                                 : "BOOT up, release\r\n");
            }
        }
    }
}
