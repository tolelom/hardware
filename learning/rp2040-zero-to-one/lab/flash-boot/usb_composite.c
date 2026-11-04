/*
 * USB 복합 장치: CDC ACM 에코(인터페이스 0·1)와 MSC 디스크(인터페이스 2).
 * CDC 두 인터페이스는 IAD(Interface Association Descriptor)로 묶고, 장치 class는
 * 0xef/0x02/0x01(IAD 사용 표시)로 둔다. EP1 IN interrupt·EP2 bulk는 CDC, EP3 bulk는 MSC.
 * CDC 처리는 usb_cdc.c, MSC 처리는 usb_msc.c와 같다. 아래는 usb_msc.c의 설명이다.
 *
 * USB 대용량 저장장치 (MSC, Bulk-Only Transport + SCSI). 128섹터(64KB) 읽기 전용 FAT12 디스크.
 * 섹터 내용은 저장하지 않고 요청이 올 때 64바이트씩 만든다.
 *   섹터 0 부트 섹터, 1·2 FAT 두 벌, 3 루트 디렉터리(16개), 4부터 데이터(클러스터 = 1섹터)
 *   README.TXT  클러스터 2
 *   PATTERN.BIN 클러스터 3부터 96개(49152바이트). 워드 i의 값은 i ^ 0xa5a5a5a5
 * MODE SENSE에서 쓰기 금지 비트를 세워 호스트가 읽기 전용으로 붙인다.
 * USB 장치 쪽 처리는 usb_cdc.c와 같은 방식이고, 인터페이스와 엔드포인트만 다르다.
 * 받은 SCSI 명령은 UART로 짧게 남긴다.
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
#define EP2_IN_CTRL 0x10u
#define EP2_OUT_CTRL 0x14u
#define EP3_IN_CTRL 0x18u
#define EP3_OUT_CTRL 0x1cu
#define EP0_IN_BUF_CTRL 0x80u
#define EP0_OUT_BUF_CTRL 0x84u
#define EP2_IN_BUF_CTRL 0x90u
#define EP2_OUT_BUF_CTRL 0x94u
#define EP3_IN_BUF_CTRL 0x98u
#define EP3_OUT_BUF_CTRL 0x9cu
#define EP0_BUF 0x100u
#define EP1_IN_BUF 0x180u
#define EP2_OUT_BUF 0x1c0u
#define EP2_IN_BUF 0x200u
#define EP3_IN_BUF 0x240u
#define EP3_OUT_BUF 0x280u

#define EP_CTRL_ENABLE (1u << 31)
#define EP_CTRL_INT_PER_BUFFER (1u << 29)
#define EP_CTRL_TYPE_BULK (2u << 26)
#define EP_CTRL_TYPE_INTERRUPT (3u << 26)

#define BUF_FULL (1u << 15)
#define BUF_DATA1 (1u << 13)
#define BUF_STALL (1u << 11)
#define BUF_AVAILABLE (1u << 10)
#define BUF_LEN_MASK 0x3ffu

#define BUFF_EP0_IN (1u << 0)
#define BUFF_EP0_OUT (1u << 1)
#define BUFF_EP2_IN (1u << 4)
#define BUFF_EP2_OUT (1u << 5)
#define BUFF_EP3_IN (1u << 6)
#define BUFF_EP3_OUT (1u << 7)

#define EP0_SIZE 64u
#define BULK_SIZE 64u

#define REQ_GET_STATUS 0x00u
#define REQ_CLEAR_FEATURE 0x01u
#define REQ_SET_ADDRESS 0x05u
#define REQ_GET_DESCRIPTOR 0x06u
#define REQ_GET_CONFIGURATION 0x08u
#define REQ_SET_CONFIGURATION 0x09u
#define REQ_MSC_GET_MAX_LUN 0xfeu
#define REQ_MSC_RESET 0xffu
#define REQ_SET_LINE_CODING 0x20u
#define REQ_GET_LINE_CODING 0x21u
#define DESC_DEVICE 1u
#define DESC_CONFIG 2u
#define DESC_STRING 3u
#define REQ_TYPE_STANDARD 0u
#define REQ_TYPE_CLASS 1u
#define REQ_DIR_IN 0x80u
#define REQ_RECIPIENT_ENDPOINT 2u

static const uint8_t device_descriptor[] = {
    18, DESC_DEVICE, 0x00, 0x02,
    0xef, 0x02, 0x01,             /* Miscellaneous, IAD 사용 */
    EP0_SIZE,
    0x09, 0x12, 0x01, 0x00,       /* VID 0x1209, PID 0x0001 (pid.codes 시험용) */
    0x00, 0x03,                   /* bcdDevice 3.00. CDC·MSC 펌웨어와 구분 */
    1, 2, 3,
    1,
};

static const uint8_t config_descriptor[] = {
    9, DESC_CONFIG, 98, 0, 3, 1, 0, 0x80, 50,
    /* IAD: 인터페이스 0부터 2개가 CDC ACM 한 기능 */
    8, 0x0b, 0, 2, 0x02, 0x02, 0x00, 0,
    /* 인터페이스 0: CDC 통신 (ACM) */
    9, 4, 0, 0, 1, 0x02, 0x02, 0x00, 0,
    5, 0x24, 0x00, 0x10, 0x01,
    5, 0x24, 0x01, 0x00, 0x01,
    4, 0x24, 0x02, 0x02,
    5, 0x24, 0x06, 0x00, 0x01,
    7, 5, 0x81, 0x03, 8, 0, 16,         /* EP1 IN interrupt */
    /* 인터페이스 1: CDC 데이터 */
    9, 4, 1, 0, 2, 0x0a, 0x00, 0x00, 0,
    7, 5, 0x02, 0x02, BULK_SIZE, 0, 0,  /* EP2 OUT bulk */
    7, 5, 0x82, 0x02, BULK_SIZE, 0, 0,  /* EP2 IN bulk */
    /* 인터페이스 2: MSC, SCSI transparent(0x06), Bulk-Only(0x50) */
    9, 4, 2, 0, 2, 0x08, 0x06, 0x50, 0,
    7, 5, 0x83, 0x02, BULK_SIZE, 0, 0,  /* EP3 IN bulk */
    7, 5, 0x03, 0x02, BULK_SIZE, 0, 0,  /* EP3 OUT bulk */
};

static const char *const strings[] = {
    0, "tolelom", "RP2040 0to1 CDC+MSC", "000000000003",
};
#define STRING_COUNT 4u

static uint8_t string_buffer[2 + 2 * 32];
static uint32_t ep0_in_pid;
static uint32_t ep0_out_pid;
static uint32_t ep3_in_pid;
static uint32_t ep3_out_pid;
static const uint8_t *ep0_tx_data;
static uint32_t ep0_tx_left;
static int ep0_tx_zlp;
static int ep0_in_data_stage;
static uint32_t pending_address;
static uint8_t configuration_value;
static const uint8_t zero_bytes[2] = {0, 0};
static uint8_t line_coding[7] = {0x00, 0xc2, 0x01, 0x00, 0, 0, 8};
static int ep0_expect_line_coding;
static uint32_t ep2_in_pid;
static uint32_t ep2_out_pid;
static uint32_t ep2_in_last_len;
static uint32_t cdc_bytes;

static void buf_ctrl_write(uint32_t offset, uint32_t value) {
    DPRAM32(offset) = value & ~BUF_AVAILABLE;
    for (volatile int i = 0; i < 4; i++) {
    }
    DPRAM32(offset) = value;
}

/* ------------------------------------------------------------ 디스크 --- */
#define SECTOR_SIZE 512u
#define SECTORS 128u
#define FAT_SECTOR 1u
#define FAT_COPIES 2u
#define ROOT_SECTOR 3u
#define DATA_SECTOR 4u
#define README_CLUSTER 2u
#define PATTERN_CLUSTER 3u
#define PATTERN_CLUSTERS 96u
#define PATTERN_BYTES (PATTERN_CLUSTERS * SECTOR_SIZE)
#define FAT_DATE (((2026u - 1980u) << 9) | (11u << 5) | 4u)

static const char readme[] =
    "RP2040 0 to 1 USB MSC\r\n"
    "Read-only FAT12, 128 sectors of 512 bytes.\r\n"
    "Sectors are generated on request; nothing is stored.\r\n"
    "PATTERN.BIN: 32-bit little-endian word i = i ^ 0xa5a5a5a5.\r\n";

static uint8_t boot_sector[64];
static uint8_t fat[SECTOR_SIZE];
static uint8_t root[SECTOR_SIZE];

static void put16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) v;
    p[1] = (uint8_t) (v >> 8);
}

static void put32(uint8_t *p, uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}

static void put32be(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t) (v >> 24);
    p[1] = (uint8_t) (v >> 16);
    p[2] = (uint8_t) (v >> 8);
    p[3] = (uint8_t) v;
}

static void copy(uint8_t *dst, const char *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (uint8_t) src[i];
    }
}

/* FAT12 항목 n은 바이트 n*3/2에서 12비트를 차지한다 */
static void fat12_set(uint32_t n, uint32_t value) {
    uint32_t at = n * 3u / 2u;
    if (n & 1u) {
        fat[at] = (uint8_t) ((fat[at] & 0x0fu) | ((value << 4) & 0xf0u));
        fat[at + 1u] = (uint8_t) (value >> 4);
    } else {
        fat[at] = (uint8_t) value;
        fat[at + 1u] = (uint8_t) ((fat[at + 1u] & 0xf0u) | ((value >> 8) & 0x0fu));
    }
}

static void dir_entry(uint8_t *e, const char *name11, uint32_t attr, uint32_t cluster,
                      uint32_t size) {
    copy(e, name11, 11);
    e[11] = (uint8_t) attr;
    put16(e + 22, 0);
    put16(e + 24, FAT_DATE);
    put16(e + 16, FAT_DATE);
    put16(e + 18, FAT_DATE);
    put16(e + 26, cluster);
    put32(e + 28, size);
}

static void disk_init(void) {
    uint8_t *b = boot_sector;
    b[0] = 0xeb;
    b[1] = 0x3c;
    b[2] = 0x90;
    copy(b + 3, "MSDOS5.0", 8);
    put16(b + 11, SECTOR_SIZE);
    b[13] = 1;                    /* 클러스터당 섹터 */
    put16(b + 14, FAT_SECTOR);    /* 예약 섹터 */
    b[16] = FAT_COPIES;
    put16(b + 17, 16);            /* 루트 항목 16개 = 1섹터 */
    put16(b + 19, SECTORS);
    b[21] = 0xf8;
    put16(b + 22, 1);             /* FAT 하나가 1섹터 */
    put16(b + 24, 1);
    put16(b + 26, 1);
    b[36] = 0x80;
    b[38] = 0x29;
    put32(b + 39, 0x20402040u);
    copy(b + 43, "RP2040 0TO1", 11);
    copy(b + 54, "FAT12   ", 8);

    fat12_set(0, 0xff8u);
    fat12_set(1, 0xfffu);
    fat12_set(README_CLUSTER, 0xfffu);
    for (uint32_t c = PATTERN_CLUSTER; c < PATTERN_CLUSTER + PATTERN_CLUSTERS; c++) {
        fat12_set(c, c + 1u == PATTERN_CLUSTER + PATTERN_CLUSTERS ? 0xfffu : c + 1u);
    }

    dir_entry(root, "RP2040 0TO1", 0x08, 0, 0);
    dir_entry(root + 32, "README  TXT", 0x01, README_CLUSTER, sizeof readme - 1u);
    dir_entry(root + 64, "PATTERN BIN", 0x01, PATTERN_CLUSTER, PATTERN_BYTES);
}

/* 섹터 lba의 offset부터 64바이트를 만든다 */
static void disk_chunk(uint32_t lba, uint32_t offset, volatile uint8_t *dst) {
    for (uint32_t i = 0; i < 64u; i++) {
        uint32_t at = offset + i;
        uint8_t v = 0;
        if (lba == 0u) {
            v = at < sizeof boot_sector ? boot_sector[at]
                : at == 510u ? 0x55u : at == 511u ? 0xaau : 0u;
        } else if (lba >= FAT_SECTOR && lba < FAT_SECTOR + FAT_COPIES) {
            v = fat[at];
        } else if (lba == ROOT_SECTOR) {
            v = root[at];
        } else if (lba >= DATA_SECTOR) {
            uint32_t cluster = lba - DATA_SECTOR + 2u;
            if (cluster == README_CLUSTER) {
                v = at < sizeof readme - 1u ? (uint8_t) readme[at] : 0u;
            } else if (cluster >= PATTERN_CLUSTER
                       && cluster < PATTERN_CLUSTER + PATTERN_CLUSTERS) {
                uint32_t pos = (cluster - PATTERN_CLUSTER) * SECTOR_SIZE + at;
                uint32_t word = (pos / 4u) ^ 0xa5a5a5a5u;
                v = (uint8_t) (word >> ((pos % 4u) * 8u));
            }
        }
        dst[i] = v;
    }
}

/* ------------------------------------------------------- Bulk-Only Transport --- */
#define CBW_SIGNATURE 0x43425355u
#define CSW_SIGNATURE 0x53425355u
#define SCSI_TEST_UNIT_READY 0x00u
#define SCSI_REQUEST_SENSE 0x03u
#define SCSI_INQUIRY 0x12u
#define SCSI_MODE_SENSE_6 0x1au
#define SCSI_START_STOP_UNIT 0x1bu
#define SCSI_PREVENT_ALLOW 0x1eu
#define SCSI_READ_FORMAT_CAPACITIES 0x23u
#define SCSI_READ_CAPACITY_10 0x25u
#define SCSI_READ_10 0x28u
#define SENSE_ILLEGAL_REQUEST 0x05u
#define SENSE_DATA_PROTECT 0x07u

enum bot_state { BOT_CBW, BOT_DATA_IN, BOT_DATA_OUT, BOT_CSW };

static enum bot_state bot_state;
static uint32_t cbw_tag;
static uint32_t cbw_length;
static uint32_t csw_residue;
static uint8_t csw_status;
static uint8_t sense_key;
static uint8_t sense_asc;
static uint8_t reply[36];
static uint32_t reply_len;
static uint32_t in_left;       /* 이번 데이터 단계에 보낼 남은 바이트 */
static uint32_t in_sent;
static int in_from_disk;
static uint32_t read_lba;
static uint32_t read_offset;
static int in_need_zlp;
static uint32_t out_left;
static uint32_t scsi_count[8]; /* TUR, SENSE, INQ, MODE, CAP, READ, 기타, 실패 */
static uint32_t read_sectors;

static void ep3_in_send(uint32_t len) {
    buf_ctrl_write(EP3_IN_BUF_CTRL, len | BUF_FULL | BUF_AVAILABLE
                   | (ep3_in_pid ? BUF_DATA1 : 0u));
    ep3_in_pid ^= 1u;
}

static void ep3_out_receive(void) {
    buf_ctrl_write(EP3_OUT_BUF_CTRL, BULK_SIZE | BUF_AVAILABLE
                   | (ep3_out_pid ? BUF_DATA1 : 0u));
    ep3_out_pid ^= 1u;
}

static void send_csw(void) {
    volatile uint8_t *p = DPRAM8(EP3_IN_BUF);
    uint8_t csw[13];
    put32(csw, CSW_SIGNATURE);
    put32(csw + 4, cbw_tag);
    put32(csw + 8, csw_residue);
    csw[12] = csw_status;
    for (uint32_t i = 0; i < 13u; i++) {
        p[i] = csw[i];
    }
    bot_state = BOT_CSW;
    ep3_in_send(13u);
}

/* 데이터 단계 다음 패킷. 다 보냈으면 짧은 패킷으로 끝을 알리고 CSW로 넘어간다 */
static void data_in_next(void) {
    if (in_left == 0u) {
        if (in_need_zlp) {
            in_need_zlp = 0;
            ep3_in_send(0u);
            return;
        }
        send_csw();
        return;
    }
    uint32_t len = in_left < BULK_SIZE ? in_left : BULK_SIZE;
    volatile uint8_t *p = DPRAM8(EP3_IN_BUF);
    if (in_from_disk) {
        disk_chunk(read_lba, read_offset, p);
        read_offset += BULK_SIZE;
        if (read_offset == SECTOR_SIZE) {
            read_offset = 0;
            read_lba++;
            read_sectors++;
        }
    } else {
        for (uint32_t i = 0; i < len; i++) {
            p[i] = reply[in_sent + i];
        }
    }
    in_left -= len;
    in_sent += len;
    ep3_in_send(len);
}

/* 장치가 보낼 바이트 수 n과 호스트가 요청한 길이로 데이터 단계를 시작한다.
   n이 요청보다 짧고 64의 배수면 길이 0 패킷으로 끝을 알린다 */
static void start_data_in(uint32_t n, int from_disk) {
    if (n > cbw_length) {
        n = cbw_length;
    }
    in_left = n;
    in_sent = 0;
    in_from_disk = from_disk;
    in_need_zlp = n < cbw_length && n % BULK_SIZE == 0u;
    csw_residue = cbw_length - n;
    bot_state = BOT_DATA_IN;
    data_in_next();
}

static void fail(uint8_t key, uint8_t asc, int dir_in) {
    sense_key = key;
    sense_asc = asc;
    csw_status = 1;
    scsi_count[7]++;
    if (cbw_length == 0u) {
        csw_residue = 0;
        send_csw();
    } else if (dir_in) {
        start_data_in(0, 0);
    } else {
        /* 호스트가 보낼 데이터를 받아 버린 뒤 CSW를 보낸다 */
        out_left = cbw_length;
        csw_residue = cbw_length;
        bot_state = BOT_DATA_OUT;
        ep3_out_receive();
    }
}

static void zero_reply(uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        reply[i] = 0;
    }
}

static void log_cbw(const uint8_t *cb) {
    uart_putc('c');
    uart_puthex(cb[0], 2);
    if (cb[0] == SCSI_READ_10) {
        uart_putc(' ');
        uart_putdec(((uint32_t) cb[2] << 24) | ((uint32_t) cb[3] << 16)
                    | ((uint32_t) cb[4] << 8) | cb[5]);
        uart_putc('+');
        uart_putdec(((uint32_t) cb[7] << 8) | cb[8]);
    }
    uart_puts("\r\n");
}

static void handle_cbw(uint32_t len) {
    volatile uint8_t *p = DPRAM8(EP3_OUT_BUF);
    uint8_t cbw[31];
    for (uint32_t i = 0; i < 31u; i++) {
        cbw[i] = p[i];
    }
    uint32_t signature = cbw[0] | ((uint32_t) cbw[1] << 8) | ((uint32_t) cbw[2] << 16)
                         | ((uint32_t) cbw[3] << 24);
    if (len != 31u || signature != CBW_SIGNATURE) {
        uart_puts("bad CBW\r\n");
        ep3_out_receive();
        return;
    }
    cbw_tag = cbw[4] | ((uint32_t) cbw[5] << 8) | ((uint32_t) cbw[6] << 16)
              | ((uint32_t) cbw[7] << 24);
    cbw_length = cbw[8] | ((uint32_t) cbw[9] << 8) | ((uint32_t) cbw[10] << 16)
                 | ((uint32_t) cbw[11] << 24);
    int dir_in = (cbw[12] & 0x80u) != 0;
    const uint8_t *cb = cbw + 15;
    csw_status = 0;
    csw_residue = 0;
    log_cbw(cb);

    switch (cb[0]) {
    case SCSI_TEST_UNIT_READY:
    case SCSI_PREVENT_ALLOW:
    case SCSI_START_STOP_UNIT:
        scsi_count[0]++;
        csw_residue = cbw_length;
        send_csw();
        break;
    case SCSI_REQUEST_SENSE:
        scsi_count[1]++;
        zero_reply(18);
        reply[0] = 0x70;          /* 현재 오류, 고정 형식 */
        reply[2] = sense_key;
        reply[7] = 10;
        reply[12] = sense_asc;
        sense_key = 0;
        sense_asc = 0;
        reply_len = 18;
        start_data_in(reply_len, 0);
        break;
    case SCSI_INQUIRY:
        scsi_count[2]++;
        zero_reply(36);
        reply[1] = 0x80;          /* 이동식 */
        reply[2] = 0x04;          /* SPC-2 */
        reply[3] = 0x02;
        reply[4] = 31;
        copy(reply + 8, "tolelom ", 8);
        copy(reply + 16, "RP2040 0to1 MSC ", 16);
        copy(reply + 32, "1.00", 4);
        start_data_in(36, 0);
        break;
    case SCSI_MODE_SENSE_6:
        scsi_count[3]++;
        zero_reply(4);
        reply[0] = 3;
        reply[2] = 0x80;          /* WP: 쓰기 금지 */
        start_data_in(4, 0);
        break;
    case SCSI_READ_CAPACITY_10:
        scsi_count[4]++;
        put32be(reply, SECTORS - 1u);
        put32be(reply + 4, SECTOR_SIZE);
        start_data_in(8, 0);
        break;
    case SCSI_READ_FORMAT_CAPACITIES:
        scsi_count[6]++;
        zero_reply(12);
        reply[3] = 8;
        put32be(reply + 4, SECTORS);
        reply[8] = 0x02;          /* 포맷된 매체 */
        reply[10] = (uint8_t) (SECTOR_SIZE >> 8);
        start_data_in(12, 0);
        break;
    case SCSI_READ_10: {
        scsi_count[5]++;
        uint32_t lba = ((uint32_t) cb[2] << 24) | ((uint32_t) cb[3] << 16)
                       | ((uint32_t) cb[4] << 8) | cb[5];
        uint32_t count = ((uint32_t) cb[7] << 8) | cb[8];
        if (lba + count > SECTORS) {
            fail(SENSE_ILLEGAL_REQUEST, 0x21, dir_in); /* LBA 범위 밖 */
            break;
        }
        read_lba = lba;
        read_offset = 0;
        start_data_in(count * SECTOR_SIZE, 1);
        break;
    }
    case 0x2au: /* WRITE(10) */
        fail(SENSE_DATA_PROTECT, 0x27, dir_in);
        break;
    default:
        scsi_count[6]++;
        fail(SENSE_ILLEGAL_REQUEST, 0x20, dir_in); /* 지원하지 않는 명령 */
        break;
    }
}

static void handle_ep3_out_done(void) {
    uint32_t len = DPRAM32(EP3_OUT_BUF_CTRL) & BUF_LEN_MASK;
    if (bot_state == BOT_DATA_OUT) {
        out_left = len < out_left ? out_left - len : 0u;
        if (out_left) {
            ep3_out_receive();
        } else {
            send_csw();
        }
        return;
    }
    handle_cbw(len);
}

static void handle_ep3_in_done(void) {
    if (bot_state == BOT_DATA_IN) {
        data_in_next();
    } else if (bot_state == BOT_CSW) {
        bot_state = BOT_CBW;
        ep3_out_receive();
    }
}

/* ------------------------------------------------------------ CDC --- */
static void ep2_out_receive(void) {
    buf_ctrl_write(EP2_OUT_BUF_CTRL, BULK_SIZE | BUF_AVAILABLE
                   | (ep2_out_pid ? BUF_DATA1 : 0u));
    ep2_out_pid ^= 1u;
}

static void ep2_in_send(uint32_t len) {
    ep2_in_last_len = len;
    buf_ctrl_write(EP2_IN_BUF_CTRL, len | BUF_FULL | BUF_AVAILABLE
                   | (ep2_in_pid ? BUF_DATA1 : 0u));
    ep2_in_pid ^= 1u;
}

static void handle_ep2_out_done(void) {
    uint32_t len = DPRAM32(EP2_OUT_BUF_CTRL) & BUF_LEN_MASK;
    for (uint32_t i = 0; i < len; i++) {
        DPRAM8(EP2_IN_BUF)[i] = DPRAM8(EP2_OUT_BUF)[i];
    }
    cdc_bytes += len;
    ep2_in_send(len);
}

/* 64바이트로 끝나면 길이 0 패킷을 붙인 뒤 다음 OUT을 받는다 (usb_cdc.c 기록) */
static void handle_ep2_in_done(void) {
    if (ep2_in_last_len == BULK_SIZE) {
        ep2_in_send(0u);
        return;
    }
    ep2_out_receive();
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

static void handle_ep0_out_done(void) {
    if (!ep0_expect_line_coding) {
        return;
    }
    ep0_expect_line_coding = 0;
    for (uint32_t i = 0; i < sizeof line_coding; i++) {
        line_coding[i] = DPRAM8(EP0_BUF)[i];
    }
    ep0_in_status();
}

static void bot_restart(void) {
    ep3_in_pid = 0u;
    ep3_out_pid = 0u;
    bot_state = BOT_CBW;
    ep3_out_receive();
}

static void handle_setup(void) {
    volatile uint8_t *p = DPRAM8(0);
    uint32_t request_type = p[0];
    uint32_t request = p[1];
    uint32_t value = p[2] | ((uint32_t) p[3] << 8);
    uint32_t index = p[4] | ((uint32_t) p[5] << 8);
    uint32_t length = p[6] | ((uint32_t) p[7] << 8);
    uint32_t type = (request_type >> 5) & 0x3u;
    uint32_t recipient = request_type & 0x1fu;
    int dir_in = (request_type & REQ_DIR_IN) != 0;

    ep0_in_pid = 1u;
    ep0_out_pid = 1u;
    ep0_expect_line_coding = 0;

    if (type == REQ_TYPE_STANDARD && request == REQ_GET_DESCRIPTOR) {
        uint32_t desc = value >> 8;
        uint32_t desc_index = value & 0xffu;
        if (desc == DESC_DEVICE) {
            ep0_in_start(device_descriptor, sizeof device_descriptor, length);
        } else if (desc == DESC_CONFIG) {
            ep0_in_start(config_descriptor, sizeof config_descriptor, length);
        } else if (desc == DESC_STRING && desc_index < STRING_COUNT) {
            ep0_in_start(string_buffer, string_descriptor(desc_index), length);
        } else {
            ep0_stall();
        }
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_ADDRESS) {
        pending_address = value & 0x7fu;
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_SET_CONFIGURATION) {
        configuration_value = (uint8_t) value;
        ep2_in_pid = 0u;
        ep2_out_pid = 0u;
        ep2_out_receive();
        bot_restart();
        ep0_in_status();
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_CONFIGURATION) {
        ep0_in_start(&configuration_value, 1u, length);
    } else if (type == REQ_TYPE_STANDARD && request == REQ_GET_STATUS) {
        ep0_in_start(zero_bytes, 2u, length);
    } else if (type == REQ_TYPE_STANDARD && request == REQ_CLEAR_FEATURE
               && recipient == REQ_RECIPIENT_ENDPOINT) {
        /* ENDPOINT_HALT 해제. 이 장치는 bulk를 STALL하지 않지만 데이터 PID는 되돌린다 */
        if ((index & 0x7fu) == 3u) {
            if (index & 0x80u) {
                ep3_in_pid = 0u;
            } else {
                ep3_out_pid = 0u;
            }
        }
        ep0_in_status();
    } else if (type == REQ_TYPE_CLASS && request == REQ_SET_LINE_CODING) {
        ep0_expect_line_coding = 1;
        ep0_out_receive();
    } else if (type == REQ_TYPE_CLASS && request == REQ_GET_LINE_CODING) {
        ep0_in_start(line_coding, sizeof line_coding, length);
    } else if (type == REQ_TYPE_CLASS && request == REQ_MSC_GET_MAX_LUN) {
        ep0_in_start(zero_bytes, 1u, length);  /* LUN 0 하나 */
    } else if (type == REQ_TYPE_CLASS && request == REQ_MSC_RESET) {
        bot_restart();
        ep0_in_status();
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

static void handle_bus_reset(void) {
    USB_ADDR_ENDP = 0u;
    pending_address = 0u;
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
    DPRAM32(EP3_IN_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                           | EP_CTRL_TYPE_BULK | EP3_IN_BUF;
    DPRAM32(EP3_OUT_CTRL) = EP_CTRL_ENABLE | EP_CTRL_INT_PER_BUFFER
                            | EP_CTRL_TYPE_BULK | EP3_OUT_BUF;
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
        if (buffers & BUFF_EP3_OUT) {
            handle_ep3_out_done();
        }
        if (buffers & BUFF_EP3_IN) {
            handle_ep3_in_done();
        }
    }
    if (status & INTS_BUS_RESET) {
        USB_SIE_STATUS = SIE_STATUS_BUS_RESET;
        handle_bus_reset();
    }
}

int main(void) {
    board_init();
    clocks_48mhz_init();
    disk_init();
    uart_puts("usb composite start\r\n");
    usb_init();
    uint32_t last_report = time_us();
    uint32_t last_sectors = 0;
    while (1) {
        usb_poll();
        if (time_us() - last_report > 5000000u && read_sectors + cdc_bytes != last_sectors) {
            last_report = time_us();
            last_sectors = read_sectors + cdc_bytes;
            uart_puts("sectors read ");
            uart_putdec(read_sectors);
            uart_puts(", failed cmds ");
            uart_putdec(scsi_count[7]);
            uart_puts(", cdc bytes echoed ");
            uart_putdec(cdc_bytes);
            uart_puts("\r\n");
        }
    }
}
