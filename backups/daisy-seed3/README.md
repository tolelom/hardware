# Daisy Seed3 공장 출하 내부 플래시 백업

기기 계획서: [devices/bass-effector.md](../../devices/bass-effector.md)
최종 갱신: 2026-09-30

## 무엇인가

보드를 처음 받은 상태의 STM32H750 내부 플래시 128 KB를 통째로 읽어낸 것이다.
이펙터 펌웨어를 덮어쓰기 전에 채취했다. 사용 영역은 앞 113,008바이트이고 나머지는 `0xFF`다.
두 보드가 같으므로 이 파일 하나로 어느 쪽이든 복원한다.

ROM DFU가 보고하는 serial(`200364500000`)은 두 보드가 같아 개체 구분에 쓸 수 없다. 보드는 물리 표시로 구분한다.

| 항목 | 값 |
|---|---|
| 대상 개체 | Seed3 1번 보드. 2번 보드의 공장 플래시도 읽어 바이트 단위로 같음을 확인했다 |
| 영역 | Internal Flash `0x08000000`, 128 KB. Option Bytes는 읽지 않았다 |
| 채취 수단 | dfu-util 0.9, STM32 ROM DFU(BOOT+RESET), WSL Ubuntu 22.04 + usbipd |
| 채취일 | 2026-09-30 |

## 파일

덤프 원본은 비공개 저장소 `hardware-private`의 같은 경로에 있다. 아래 명령은 그 파일을 이 폴더에 둔 상태를 기준으로 한다.

| 파일 | 크기 | 내용 |
|---|---|---|
| `seed3-factory-internal.bin` | 131,072 | 원본 그대로. 파일 오프셋 0이 주소 `0x08000000`. 비공개 저장소에 있음 |
| `SHA256SUMS` | | 해시 |

## 복원

DFU 모드(BOOT를 누른 채 RESET)에서:

```
dfu-util -d ,0483:df11 -a 0 -s 0x08000000:leave -D seed3-factory-internal.bin
```
