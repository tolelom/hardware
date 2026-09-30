# Daisy Seed3 공장 출하 내부 플래시 백업

기기 계획서: [devices/bass-effector.md](../../devices/bass-effector.md)
최종 갱신: 2026-09-30

## 무엇인가

보드를 처음 받은 상태의 STM32H750 내부 플래시 128 KB를 통째로 읽어낸 것이다.
이펙터 펌웨어를 덮어쓰기 전에 채취했다. 사용 영역은 앞 113,008바이트이고 나머지는 `0xFF`다.

| 항목 | 값 |
|---|---|
| 대상 개체 | DFU serial `200364500000` (Seed3 두 개 중 첫 번째) |
| 영역 | Internal Flash `0x08000000`, 128 KB. Option Bytes는 읽지 않았다 |
| 채취 수단 | dfu-util 0.9, STM32 ROM DFU(BOOT+RESET), WSL Ubuntu 22.04 + usbipd |
| 채취일 | 2026-09-30 |

## 파일

| 파일 | 크기 | 내용 |
|---|---|---|
| `seed3-200364500000-factory-internal.bin` | 131,072 | 원본 그대로. 파일 오프셋 0이 주소 `0x08000000` |
| `SHA256SUMS` | | 해시 |

## 복원

DFU 모드(BOOT를 누른 채 RESET)에서:

```
dfu-util -d ,0483:df11 -a 0 -s 0x08000000:leave -D seed3-200364500000-factory-internal.bin
```
