#!/usr/bin/env python3
"""RP2040 플래시 덤프 검증. 사용법: verify.py <덤프.bin> [기준.bin]"""
import sys, os, binascii, struct, hashlib

def bitrev(x, w):
    return int("{:0{w}b}".format(x, w=w)[::-1], 2)

def boot2_crc(body, seed=0xffffffff):
    return bitrev((binascii.crc32(bytes(bitrev(b, 8) for b in body),
                                  seed ^ 0xffffffff) ^ 0xffffffff) & 0xffffffff, 32)

if len(sys.argv) < 2:
    sys.exit(__doc__)

d = open(sys.argv[1], "rb").read()
ref = open(sys.argv[2], "rb").read() if len(sys.argv) > 2 else None
ok = True

def check(label, passed, detail=""):
    global ok
    ok = ok and passed
    print(f"  [{'OK' if passed else '실패':^4}] {label:24s} {detail}")

print(f"\n검증 대상: {sys.argv[1]}\n")

check("크기 2MB", len(d) == 2*1024*1024, f"{len(d):,} bytes")

digest = hashlib.sha256(d).hexdigest()
print(f"  [    ] {'sha256':24s} {digest}")

body, stored = d[:252], struct.unpack("<L", d[252:256])[0]
calc = boot2_crc(body)
check("boot2 CRC32", stored == calc, f"저장 {stored:#010x} / 계산 {calc:#010x}")

last = len(d)
while last > 0 and d[last-1] == 0xFF:
    last -= 1
print(f"  [    ] {'내용 범위':24s} {last:#x} ({last:,} bytes), 이후 0xFF")

if ref is not None:
    check("원본과 전체 일치", d == ref,
          "동일" if d == ref else f"{sum(a!=b for a,b in zip(d,ref)):,} 바이트 다름")

print(f"\n종합: {'통과' if ok else '실패'}\n")
sys.exit(0 if ok else 1)
