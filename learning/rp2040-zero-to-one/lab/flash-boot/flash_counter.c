/*
 * flash 마지막 섹터에 부팅 기록을 남긴다 (데이터시트 2.8.3.1.3).
 * 섹터(offset 0x1ff000, 4KiB)를 워드 1024개로 보고, 부팅마다 첫 빈 워드(0xffffffff)에
 * 그 순번을 쓴다. 쓰기는 지우지 않고 1을 0으로만 바꾸므로 섹터가 찰 때까지 지울
 * 필요가 없다. 앞 워드가 순번과 다르거나 섹터가 차면 지우고 0부터 쓴다.
 *
 * flash를 지우거나 쓰는 동안 XIP는 쓸 수 없으므로, 그 구간의 함수와 데이터는 SRAM에
 * 두고 인터럽트를 끈다. 끝나면 SRAM에 복사해 둔 boot2를 불러 빠른 XIP로 되돌린다.
 */
#include "board.h"

#define XIP_BASE 0x10000000u
#define SECTOR_OFFSET 0x1ff000u
#define SECTOR_SIZE 4096u
#define PAGE_SIZE 256u
#define WORDS (SECTOR_SIZE / 4u)
#define ERASED 0xffffffffu
#define BOOT2_SIZE 256u

typedef void (*rom_void_fn)(void);
typedef void (*rom_erase_fn)(uint32_t, uint32_t, uint32_t, uint8_t);
typedef void (*rom_program_fn)(uint32_t, const uint8_t *, uint32_t);

struct flash_rom {
    rom_void_fn connect;
    rom_void_fn exit_xip;
    rom_erase_fn erase;
    rom_program_fn program;
    rom_void_fn flush_cache;
};

static struct flash_rom rom;
static uint32_t boot2_copy[BOOT2_SIZE / 4u];
static uint32_t page[PAGE_SIZE / 4u];

/* flash 코드에서 SRAM 함수까지는 bl이 닿지 않아 long_call로 부른다 */
#define RAMFUNC __attribute__((section(".ramfunc"), noinline, long_call))

/* erase가 1이면 섹터를 지운다. page_offset이 ERASED가 아니면 page를 그 자리에 쓴다 */
RAMFUNC static void flash_update(int erase, uint32_t page_offset) {
    rom.connect();
    rom.exit_xip();
    if (erase) {
        rom.erase(SECTOR_OFFSET, SECTOR_SIZE, 1u << 16, 0xd8u);
    }
    if (page_offset != ERASED) {
        rom.program(page_offset, (const uint8_t *) page, PAGE_SIZE);
    }
    rom.flush_cache();
    /* boot2는 lr이 0이 아니면 VTOR을 건드리지 않고 돌아온다. +1은 Thumb 비트 */
    ((rom_void_fn) ((uint32_t) boot2_copy + 1u))();
}

static void flash_rom_init(void) {
    rom.connect = (rom_void_fn) rom_func_lookup(ROM_CODE('I', 'F'));
    rom.exit_xip = (rom_void_fn) rom_func_lookup(ROM_CODE('E', 'X'));
    rom.erase = (rom_erase_fn) rom_func_lookup(ROM_CODE('R', 'E'));
    rom.program = (rom_program_fn) rom_func_lookup(ROM_CODE('R', 'P'));
    rom.flush_cache = (rom_void_fn) rom_func_lookup(ROM_CODE('F', 'C'));
    const uint32_t *boot2 = (const uint32_t *) XIP_BASE;
    for (uint32_t i = 0; i < BOOT2_SIZE / 4u; i++) {
        boot2_copy[i] = boot2[i];
    }
}

/* 앞에서부터 순번이 이어진 워드 수. 순번이 깨져 있으면 WORDS + 1 */
static uint32_t used_words(const volatile uint32_t *sector) {
    for (uint32_t i = 0; i < WORDS; i++) {
        if (sector[i] == ERASED) {
            for (uint32_t j = i; j < WORDS; j++) {
                if (sector[j] != ERASED) {
                    return WORDS + 1u;
                }
            }
            return i;
        }
        if (sector[i] != i) {
            return WORDS + 1u;
        }
    }
    return WORDS;
}

int main(void) {
    board_init();
    flash_rom_init();
    const volatile uint32_t *sector =
        (const volatile uint32_t *) (XIP_BASE + SECTOR_OFFSET);

    uint32_t used = used_words(sector);
    int erase = used >= WORDS;
    uint32_t index = erase ? 0u : used;
    uart_puts("flash counter: ");
    if (used > WORDS) {
        uart_puts("sector not in our format, erase\r\n");
    } else if (used == WORDS) {
        uart_puts("sector full, erase\r\n");
    } else {
        uart_puts("previous boots recorded ");
        uart_putdec(used);
        uart_puts("\r\n");
    }

    uint32_t page_offset = SECTOR_OFFSET + (index * 4u / PAGE_SIZE) * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE / 4u; i++) {
        page[i] = ERASED;
    }
    page[index % (PAGE_SIZE / 4u)] = index;

    uint32_t start = time_us();
    __asm volatile("cpsid i");
    flash_update(erase, page_offset);
    __asm volatile("cpsie i");
    uint32_t took = time_us() - start;

    uint32_t after = used_words(sector);
    uart_puts("wrote index ");
    uart_putdec(index);
    uart_puts(erase ? " after erase, " : ", ");
    uart_putdec(took);
    uart_puts(" us, now recorded ");
    uart_putdec(after);
    uart_puts(after == index + 1u ? " ok\r\n" : " MISMATCH\r\n");
    while (1) {
    }
}
