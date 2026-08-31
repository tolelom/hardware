# XIAO RP2040 공장 출하 플래시 백업

부품 기준 문서: [parts/xiao-rp2040.md](../../parts/xiao-rp2040.md)
최종 갱신: 2026-08-31

## 무엇인가

보드를 처음 받은 상태의 외부 QSPI 플래시 2 MB를 통째로 읽어낸 것이다.
펌웨어를 덮어쓰기 전에 채취했다.

| 항목 | 값 |
|---|---|
| 대상 개체 | flash id `0x4150345236383019` |
| 칩 | RP2040 revision B2 |
| 플래시 | Winbond W25Q080 계열 2 MB |
| 채취 수단 | picotool 2.3.0, PICOBOOT 인터페이스 경유 |
| 채취일 | 2026-08-31 |

## 파일

| 파일 | 크기 | 내용 |
|---|---|---|
| `factory-flash.bin` | 2,097,152 | 원본 그대로. 파일 오프셋 0이 주소 `0x10000000` |
| `SHA256SUMS` | | 해시 |
| `verify.py` | | 덤프 검증. 크기, boot2 CRC32, 내용 범위, 파일 간 대조 |

복구용 uf2는 두지 않는다. bin에서 바이트 단위로 같게 재생성되기 때문이다.

```
picotool uf2 convert factory-flash.bin -t bin factory-flash.uf2 -t uf2 -o 0x10000000
```

## 검증하기

```
python3 verify.py <덤프.bin> factory-flash.bin
```

두 번째 인자를 주면 `factory-flash.bin`과 전체 대조까지 한다.

## 복구 절차

1. B(부트)를 누른 채 R(리셋). `RPI-RP2` 드라이브가 뜬다
2. 위 명령으로 uf2를 만든다
3. uf2를 드라이브에 복사한다

부트로더 진입 조건은 부품 기준 문서에 있다.

## 검증한 것

- `picotool save -a -v`의 재읽기 대조 통과
- boot2 CRC32 검증 통과. 저장값 `0x7a4eb274`, 계산값 일치.
  비트반전 변종이며 시드는 `0xffffffff`, 대상은 앞 252바이트다
- boot2 256바이트가 pico-sdk 2.3.0 `boot2_w25q080` 빌드 결과와 완전 일치
- 벡터 테이블 `0x100`: 초기 스택 포인터 `0x20042000`, 리셋 핸들러 `0x100001e3`
- 내용은 오프셋 `0x13900`(80,128 바이트)까지다. 나머지는 `0xFF`
- `picotool uf2 convert`로 bin에서 만든 uf2가 채취 시 저장한 uf2와 바이트 단위로 같다

## 공장 펌웨어의 정체

arduino-pico 코어 4.4.0으로 빌드된 Arduino 스케치다.
바이너리에 빌드 경로가 남아 있다.

```
C:\Users\Ming\AppData\Local\Arduino15\packages\rp2040\hardware\rp2040\4.4.0\cores\rp2040\PIOProgram.cpp
```

`PIOProgram.cpp`가 링크돼 있다. 어느 주변장치에 PIO를 쓰는지는 확인 필요.

pico-sdk 직접 빌드가 아니라 `picotool info`에 프로그램 메타데이터가 뜨지 않는다.

## 확인 필요

- 복구 경로. 이 백업으로 실제 원상복구가 되는지 시험하지 않았다
- 바이너리 안의 `Init Ready` 문자열이 실제로 시리얼로 출력되는지.
  앱 모드에서 CDC 시리얼은 뜨지만 붙어 있는 동안 받은 것은 없었다.
  USB CDC는 호스트가 포트를 열기 전에 쓴 것을 버리므로 이 관측으로는
  출력이 없다고 단정할 수 없다

## 재현

2026-08-31에 독립으로 한 번 더 채취해 대조했다. 사이에 USB를 뽑았다 꽂아
공장 펌웨어를 앱 모드로 한 번 부팅시켰다.

두 덤프의 sha256이 같았다. 이 범위에서 읽기 결과는 재현되며, 공장 펌웨어를
한 번 부팅시킨 것으로는 플래시가 변하지 않았다.
