/*
 * lab/flash-boot 프로그램의 공통 부분. 주소와 비트는 RP2040 데이터시트 기준이고
 * 괄호 안은 절 번호다. board_init()은 clk_sys 125MHz, clk_peri 12MHz(XOSC),
 * UART0 115200, TIMER 1us를 준비하고, UART0로 "BOOTSEL"을 받으면 BOOTSEL
 * 모드로 재부팅하는 인터럽트를 켠다.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *) (addr))
#define ALIAS_XOR 0x1000u
#define ALIAS_SET 0x2000u
#define ALIAS_CLR 0x3000u

/* RESETS (2.14) */
#define RESETS_BASE 0x4000c000u
#define RESETS_RESET_SET REG(RESETS_BASE + ALIAS_SET)
#define RESETS_RESET_CLR REG(RESETS_BASE + ALIAS_CLR)
#define RESETS_RESET_DONE REG(RESETS_BASE + 0x08u)
#define RESETS_ADC (1u << 0)
#define RESETS_DMA (1u << 2)
#define RESETS_IO_BANK0 (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)
#define RESETS_PIO0 (1u << 10)
#define RESETS_PLL_SYS (1u << 12)
#define RESETS_PLL_USB (1u << 13)
#define RESETS_PWM (1u << 14)
#define RESETS_RTC (1u << 15)
#define RESETS_TIMER (1u << 21)
#define RESETS_UART0 (1u << 22)
#define RESETS_USBCTRL (1u << 24)

/* CLOCKS (2.15) */
#define CLOCKS_BASE 0x40008000u
#define CLK_REF_CTRL REG(CLOCKS_BASE + 0x30u)
#define CLK_REF_SELECTED REG(CLOCKS_BASE + 0x38u)
#define CLK_SYS_CTRL REG(CLOCKS_BASE + 0x3cu)
#define CLK_SYS_SELECTED REG(CLOCKS_BASE + 0x44u)
#define CLK_PERI_CTRL REG(CLOCKS_BASE + 0x48u)
#define CLK_USB_CTRL REG(CLOCKS_BASE + 0x54u)
#define CLK_ADC_CTRL REG(CLOCKS_BASE + 0x60u)
#define CLK_GEN_ENABLE (1u << 11)

/* IO_BANK0 (2.19.6.1), PADS_BANK0 (2.19.6.3) */
#define GPIO_CTRL(n) REG(0x40014004u + (n) * 0x8u)
#define PADS_GPIO(n) REG(0x4001c004u + (n) * 0x4u)
#define FUNCSEL_UART 2u
#define FUNCSEL_PWM 4u
#define FUNCSEL_SIO 5u
#define FUNCSEL_PIO0 6u
#define FUNCSEL_NULL 0x1fu

/* SIO (2.3.1) */
#define SIO_BASE 0xd0000000u
#define SIO_CPUID REG(SIO_BASE + 0x000u)
#define SIO_GPIO_IN REG(SIO_BASE + 0x004u)
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x014u)
#define SIO_GPIO_OUT_CLR REG(SIO_BASE + 0x018u)
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x01cu)
#define SIO_GPIO_OE_SET REG(SIO_BASE + 0x024u)

/* UART0 (4.2) */
#define UART0_BASE 0x40034000u
#define UART0_DR REG(UART0_BASE + 0x000u)
#define UART0_FR REG(UART0_BASE + 0x018u)
#define UART0_IMSC REG(UART0_BASE + 0x038u)
#define UART0_ICR REG(UART0_BASE + 0x044u)
#define UART0_DMACR REG(UART0_BASE + 0x048u)
#define UART_FR_RXFE (1u << 4)
#define UART_FR_TXFF (1u << 5)
#define UART_FR_TXFE (1u << 7)

/* TIMER (4.6) */
#define TIMER_TIMERAWL REG(0x40054000u + 0x28u)

/* NVIC (2.4.8) */
#define NVIC_ISER REG(0xe000e100u)
#define NVIC_ICER REG(0xe000e180u)
#define NVIC_ICPR REG(0xe000e280u)

/* RGB2 (회로도 v1.3): active low */
#define LED_GREEN 16u
#define LED_RED 17u
#define LED_BLUE 25u

#define CLK_SYS_HZ 125000000u
#define XOSC_HZ 12000000u

void board_init(void);
void unreset(uint32_t blocks);
void reset_block(uint32_t blocks);
void delay(uint32_t count);

void uart_putc(char c);
void uart_puts(const char *s);
void uart_putdec(uint32_t value);
void uart_puthex(uint32_t value, int digits);
void uart_flush(void);

uint32_t time_us(void);
void sleep_us(uint32_t us);

void *rom_func_lookup(uint32_t code);
#define ROM_CODE(c1, c2) ((uint32_t) (c1) | ((uint32_t) (c2) << 8))
void reset_to_bootsel(void);

void led_init(uint32_t pin);

/* PLL_USB 48MHz를 켜고 clk_usb, clk_adc에 연결한다 */
void clocks_48mhz_init(void);

#endif
