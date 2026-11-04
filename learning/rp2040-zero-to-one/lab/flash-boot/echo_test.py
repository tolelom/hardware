"""CDC 에코 시험. 사용법: python3 -I echo_test.py /dev/ttyACMn"""
import os
import sys
import time
import tty


def drain(fd: int) -> None:
    try:
        while os.read(fd, 4096):
            pass
    except BlockingIOError:
        pass


def roundtrip(fd: int, data: bytes, timeout: float) -> tuple[bytes, float]:
    sent = 0
    got = b""
    start = time.monotonic()
    while (sent < len(data) or len(got) < len(data)) \
            and time.monotonic() - start < timeout:
        if sent < len(data):
            try:
                sent += os.write(fd, data[sent:sent + 4096])
            except BlockingIOError:
                pass
        try:
            got += os.read(fd, 65536)
        except BlockingIOError:
            time.sleep(0.0005)
    return got, time.monotonic() - start


def main() -> None:
    fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd)
    time.sleep(0.2)
    drain(fd)
    cases = [b"hello", b"x" * 63, b"y" * 64, b"z" * 65, b"w" * 128,
             bytes(range(256)) * 4, os.urandom(64 * 1024)]
    for data in cases:
        got, dt = roundtrip(fd, data, 10.0)
        result = "OK" if got == data else f"MISMATCH ({len(got)} bytes back)"
        rate = len(data) / dt / 1024
        print(f"{len(data):6d} bytes  {result}  {dt * 1000:8.1f} ms  "
              f"{rate:6.0f} KiB/s")
        drain(fd)


if __name__ == "__main__":
    main()
