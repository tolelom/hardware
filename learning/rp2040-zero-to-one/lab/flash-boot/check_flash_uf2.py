"""uf2가 RP2040 flash 부팅 조건을 만족하는지 확인한다.

사용법: python3 check_flash_uf2.py <firmware.uf2> <__image_end 주소(16진수)>
기준: RP2040 데이터시트 2.8.1.3(boot2, CRC32), 2.8.4.2(UF2),
      pico-sdk boot2_helpers/exit_from_boot2.S(0x10000100 벡터 테이블)
"""
import struct
import sys

BLOCK_SIZE = 512
MAGIC_START0 = 0x0A324655
MAGIC_START1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
FLAG_FAMILY_ID_PRESENT = 0x00002000
RP2040_FAMILY_ID = 0xE48BFF56
PAYLOAD_SIZE = 256
FLASH_START = 0x10000000
FLASH_END = FLASH_START + 2 * 1024 * 1024
SRAM_START = 0x20000000
SRAM_END = 0x20042000
BOOT2_SIZE = 256
VECTOR_OFFSET = 0x100


def fail(message: str) -> None:
    sys.exit(f"FAIL {message}")


def read_image(data: bytes) -> bytes:
    """uf2 블록을 주소 순서로 이어 0x10000000부터의 바이트열로 만든다."""
    if len(data) == 0 or len(data) % BLOCK_SIZE != 0:
        fail(f"uf2 크기 {len(data)}가 {BLOCK_SIZE}의 배수가 아님")
    payloads: dict[int, bytes] = {}
    for offset in range(0, len(data), BLOCK_SIZE):
        block = data[offset:offset + BLOCK_SIZE]
        start0, start1, flags, target, size, _, _, family = struct.unpack_from(
            "<8I", block)
        (end,) = struct.unpack_from("<I", block, BLOCK_SIZE - 4)
        index = offset // BLOCK_SIZE
        if (start0, start1, end) != (MAGIC_START0, MAGIC_START1, MAGIC_END):
            fail(f"블록 {index}: 매직 넘버 불일치")
        if not flags & FLAG_FAMILY_ID_PRESENT or family != RP2040_FAMILY_ID:
            fail(f"블록 {index}: family id 0x{family:08x}")
        if size != PAYLOAD_SIZE:
            fail(f"블록 {index}: payload {size}바이트")
        if not FLASH_START <= target <= FLASH_END - size:
            fail(f"블록 {index}: 주소 0x{target:08x}가 flash 범위 밖")
        payloads[target] = block[32:32 + size]
    image = bytearray()
    for target in sorted(payloads):
        if target != FLASH_START + len(image):
            fail(f"0x{FLASH_START + len(image):08x}부터 블록이 비어 있음")
        image += payloads[target]
    return bytes(image)


def boot2_crc(data: bytes) -> int:
    """데이터시트 2.8.1.3.1: poly 0x04c11db7, init 0xffffffff, 반사 없음."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc


def main() -> None:
    if len(sys.argv) != 3 or not sys.argv[2]:
        sys.exit("사용법: check_flash_uf2.py <uf2> <__image_end 주소>")
    with open(sys.argv[1], "rb") as f:
        image = read_image(f.read())
    image_end = int(sys.argv[2], 16)

    (stored,) = struct.unpack_from("<I", image, BOOT2_SIZE - 4)
    computed = boot2_crc(image[:BOOT2_SIZE - 4])
    if stored != computed:
        fail(f"boot2 CRC 0x{stored:08x} != 계산값 0x{computed:08x}")
    print(f"OK  boot2 CRC 0x{stored:08x}")

    sp, reset = struct.unpack_from("<II", image, VECTOR_OFFSET)
    if not SRAM_START < sp <= SRAM_END or sp % 8 != 0:
        fail(f"초기 SP 0x{sp:08x}가 SRAM 범위 밖이거나 8바이트 정렬 아님")
    if not reset & 1:
        fail(f"reset 0x{reset:08x}의 Thumb 비트(bit 0)가 0")
    entry = reset & ~1
    if not FLASH_START + VECTOR_OFFSET < entry < FLASH_START + len(image):
        fail(f"reset 0x{reset:08x}가 이미지 밖")
    print(f"OK  0x10000100 벡터: SP 0x{sp:08x}, reset 0x{reset:08x}")

    if FLASH_START + len(image) < image_end:
        fail(f"이미지 끝 0x{FLASH_START + len(image):08x} < "
             f"__image_end 0x{image_end:08x} (.data 초기값 누락)")
    print(f"OK  이미지 {len(image)}바이트, .data 초기값까지 포함")


if __name__ == "__main__":
    main()
