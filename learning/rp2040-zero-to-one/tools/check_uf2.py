"""uf2가 RP2040 SRAM 전용 바이너리 조건을 만족하는지 확인한다.

사용법: python3 check_uf2.py <firmware.uf2> <_start 주소(16진수)>
기준: RP2040 데이터시트 2.8.4.2 UF2 Format Details
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
SRAM_START = 0x20000000
SRAM_END = 0x20042000


def block_addresses(data: bytes) -> list[int]:
    if len(data) == 0 or len(data) % BLOCK_SIZE != 0:
        sys.exit(f"FAIL uf2 크기 {len(data)}가 {BLOCK_SIZE}의 배수가 아님")
    addresses = []
    for offset in range(0, len(data), BLOCK_SIZE):
        block = data[offset:offset + BLOCK_SIZE]
        start0, start1, flags, target, size, _, _, family = struct.unpack_from(
            "<8I", block)
        (end,) = struct.unpack_from("<I", block, BLOCK_SIZE - 4)
        index = offset // BLOCK_SIZE
        if (start0, start1, end) != (MAGIC_START0, MAGIC_START1, MAGIC_END):
            sys.exit(f"FAIL 블록 {index}: 매직 넘버 불일치")
        if not flags & FLAG_FAMILY_ID_PRESENT or family != RP2040_FAMILY_ID:
            sys.exit(f"FAIL 블록 {index}: family id 0x{family:08x}")
        if size != PAYLOAD_SIZE:
            sys.exit(f"FAIL 블록 {index}: payload {size}바이트")
        if not SRAM_START <= target <= SRAM_END - size:
            sys.exit(f"FAIL 블록 {index}: 주소 0x{target:08x}가 SRAM 범위 밖")
        addresses.append(target)
    return addresses


def main() -> None:
    if len(sys.argv) != 3 or not sys.argv[2]:
        sys.exit("사용법: check_uf2.py <uf2> <_start 주소>")
    with open(sys.argv[1], "rb") as f:
        addresses = block_addresses(f.read())
    entry = min(addresses)
    start = int(sys.argv[2], 16)
    if entry != start:
        sys.exit(f"FAIL 진입 주소 0x{entry:08x} != _start 0x{start:08x}")
    print(f"OK  블록 {len(addresses)}개, 전부 SRAM 범위, "
          f"family 0x{RP2040_FAMILY_ID:08x}")
    print(f"OK  진입 주소 0x{entry:08x} = _start")


if __name__ == "__main__":
    main()
