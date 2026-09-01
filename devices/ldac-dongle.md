# LDAC 송신 동글 계획

iPhone(Lightning)에서 고음질 Bluetooth 송신을 하는 거치형 동글을 만든다.
음악 전용이다. HFP(통화)는 지원하지 않고 영상 립싱크 어긋남은 감수한다.
최종 갱신: 2026-09-01

## 구성 (2026-08-14 확정)

```
iPhone - Lightning-USB3 카메라 어댑터(CCK) - 보드(UAC2 gadget, USB DAC로 인식) - LDAC 송출
```

## 코덱 결론

- 자작 가능한 고음질 코덱은 LDAC(AOSP 오픈)과 LC3(LE Audio)뿐이다.
  aptX Adaptive/Lossless는 인코더 비공개라 자작이 안 된다.
- LDAC 우선. LC3는 보류: 음질은 본전이고 스택이 실험 단계이며, 라즈베리파이
  온보드 BT로는 안 되고 Intel AX210이 필요하다.
- 보유 이어폰 MTW4(Sennheiser)는 LDAC 미지원이라 수신기가 따로 필요하다.
  수신기 후보: EarFun Air Pro 4+ (LDAC·aptX Lossless·LC3 지원).
- LHDC는 인코더 소스가 공개돼 있으나 PipeWire 플러그인이 없다. 플러그인 자작은
  2차 프로젝트 후보다(오픈소스 기여 거리).

## 보드: PocketBeagle 2 확정 (2026-09-01)

최종 보드는 PocketBeagle 2다. 동글의 신호 경로는 USB 가젯 입력에서 BT 출력으로
가고 I2S를 거치지 않으므로, 오디오 플레이어(McASP·DAC) 선행이 필요 없다.

- USB0 가젯은 동작 확인됨. 인코딩은 리눅스(PipeWire)가 하므로 BT 동글은 일반
  컨트롤러면 된다.
- 필요한 하드웨어 작업은 USB1 호스트 배선(헤더의 D+·D-·VBUS·DRVVBUS에 소켓)
  하나다.
- 거치형이라 부팅 시간·배터리 압박이 플레이어보다 약하다. 부팅 단축은 2차
  계획(Buildroot)에서 다룬다.

원안이던 Zero 2 W는 내려놓는다. 품절 교착이 길고(2026-08-14 rpilocator 확인,
2026-09-01 재확인에서도 품귀 지속·재입고 시 수 시간 내 소진), PB2 확정으로
필요가 없어졌다.

Pi 3B+는 BT 송신(1~2단계)은 되지만 USB가 허브(LAN7515) 뒤라 gadget 모드가
안 돼 최종 보드는 못 된다. 검증용으로만 쓴다.

## 단계 (각 단계 검증 후 다음 지출)

| # | 단계 | 지출 |
|---|---|---|
| 1 | Pi 3B+로 MTW4 상대 aptX·SBC-XQ 송신 파이프라인 검증 | 0 |
| 2 | LDAC 수신기 구매 후 LDAC 송신 검증 | 수신기 |
| 3 | PB2 이식: USB1 배선과 BT 동글, CCK 어댑터로 iPhone 연결 | 소켓·동글·어댑터 |

1단계 판정: 헤드리스 부팅(OS Lite 64bit, Imager에서 WiFi·SSH 심기) -
pipewire·libspa-bluetooth 설치 - MTW4 페어링 - `wpctl inspect`로 협상 코덱
확인 - `pw-play`로 한 곡 무결점 재생.

Pi 3B+는 최근 켜본 기록이 없어 SD 내용부터 확인한다.
부품 기준 문서: [parts/rpi-3bplus.md](../parts/rpi-3bplus.md)

## 리스크

- USB-BT 클럭 드리프트. 가젯 클럭과 BT 클럭이 다른 소스라 비동기 리샘플링이
  필요하고, UAC2 async 피드백이 관건이며 커널 버전을 탄다.

## 2차 계획

Raspberry Pi OS로 완주한 뒤 Buildroot 최소 이미지로 재제작한다(부팅 수 초,
read-only 루트).

## 확인 안 된 것

- 맥북 대비 iPhone 음질 차이의 원인. 맥에서 option+BT 클릭으로 협상 코덱을
  확인하면 판정된다.
- 보유 중인 C타입 동글 모델.
