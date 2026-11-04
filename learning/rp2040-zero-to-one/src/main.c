#include <stdint.h>

#define RESETS_BASE 0x4000c000u
#define RESETS_RESET_CLR (*(volatile uint32_t *) (RESETS_BASE + 0x3000u + 0x0u))
#define RESETS_RESET_DONE (*(volatile uint32_t *) (RESETS_BASE + 0x08u))
#define RESETS_IO_BANK0 (1u << 5)
#define RESETS_PADS_BANK0 (1u << 8)

#define IO_BANK0_BASE 0x40014000u
#define GPIO_CTRL(n) (*(volatile uint32_t *) (IO_BANK0_BASE + 0x04u + (n) * 0x8u))
#define FUNCSEL_SIO 5u

#define SIO_BASE 0xd0000000u
#define SIO_GPIO_OUT_SET (*(volatile uint32_t *) (SIO_BASE + 0x14u))
#define SIO_GPIO_OUT_XOR (*(volatile uint32_t *) (SIO_BASE + 0x1cu))
#define SIO_GPIO_OE_SET (*(volatile uint32_t *) (SIO_BASE + 0x24u))

#define LED_RED 17u
#define BLINK_DELAY 200000u

int counter = 7;
int zeroed;

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
    }
}

int main(void) {
    uint32_t blocks = RESETS_IO_BANK0 | RESETS_PADS_BANK0;
    RESETS_RESET_CLR = blocks;
    while ((RESETS_RESET_DONE & blocks) != blocks) {
    }

    GPIO_CTRL(LED_RED) = FUNCSEL_SIO;

    SIO_GPIO_OUT_SET = 1u << LED_RED;
    SIO_GPIO_OE_SET = 1u << LED_RED;

    while (1) {
        SIO_GPIO_OUT_XOR = 1u << LED_RED;
        delay(BLINK_DELAY);
        counter++;
        zeroed++;
    }
}
