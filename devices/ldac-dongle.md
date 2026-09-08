# LDAC 송신 동글 계획

iPhone(Lightning)에서 고음질 Bluetooth 송신을 하는 거치형 동글을 만든다.
음악 전용이다. HFP(통화)는 지원하지 않고 영상 립싱크 어긋남은 감수한다.
부품 기준 문서: [parts/pocketbeagle2.md](../parts/pocketbeagle2.md), [parts/rpi-3bplus.md](../parts/rpi-3bplus.md)
최종 갱신: 2026-09-08

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
  수신기 후보: EarFun Air Pro 4+. 제조사 페이지 기준 LDAC, aptX Lossless, aptX Adaptive,
  LC3(LE Audio), AAC, SBC 지원. 칩은 Qualcomm QCC3091, Bluetooth 6.0 (2026-09-07 확인).
  "+"가 없는 Air Pro 4는 LDAC 지원 여부가 다르므로 구매 시 품명을 확인한다.
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

1단계 판정: 헤드리스 부팅(Raspberry Pi OS Lite 64bit. 2025-10부터 Trixie 기반. Imager에서 WiFi·SSH 심기) -
`pipewire`, `wireplumber`, `libspa-0.2-bluetooth` 설치 - MTW4 페어링 -
`wpctl inspect`로 협상 코덱 확인 - `pw-play`로 한 곡 무결점 재생.

코덱 라이브러리는 따로 설치할 것이 없다. Debian의 `libspa-0.2-bluetooth`가
`libldacbt-enc2`, `libldacbt-abr2`, `libfreeaptx0`, `liblc3-1`을 의존성으로 끌어온다.
bookworm과 trixie 양쪽에서 확인했다(2026-09-07, packages.debian.org). LDAC 인코딩은
이 패키지 조합으로 2단계까지 그대로 간다. Pi OS Lite에는 PipeWire가 기본 설치돼 있지
않을 수 있어 위 세 패키지를 명시해서 설치한다.

MTW4의 협상 코덱은 aptX나 SBC-XQ가 될 텐데, 어느 쪽이 잡히는지는 `wpctl inspect`
출력의 `api.bluez5.codec`으로 본다. 협상은 수신기가 결정하므로 강제하려면 wireplumber
설정에서 코덱 우선순위를 바꾼다.

1단계 명령 순서. 문서 기준이고 돌려보면 이 절을 고친다.

```
sudo apt install pipewire wireplumber libspa-0.2-bluetooth
systemctl --user enable --now pipewire wireplumber
bluetoothctl            # power on, scan on, pair <MAC>, trust <MAC>, connect <MAC>
wpctl status            # Sinks 아래에 이어폰이 뜨는지
wpctl inspect <sink id> | grep -E 'api.bluez5.codec|device.profile'
pw-play test.flac       # 또는 wpctl set-default <sink id> 뒤에 아무 플레이어
```

코덱을 고정하려면 `~/.config/wireplumber/wireplumber.conf.d/` 아래 파일에
`monitor.bluez.properties = { bluez5.codecs = [ aptx ] }` 블록을 둔다. 키 이름과 블록 구조는
WirePlumber 0.5 문서 기준이며 Trixie의 버전에서 같은지 확인한다.

OS Lite는 데스크톱 세션이 없으므로 `systemctl --user`가 돌려면 로그인 세션이 필요하다.
ssh로 들어가서 하면 되고, 부팅 시 자동 기동은 `loginctl enable-linger`로 건다.

Pi 3B+는 최근 켜본 기록이 없어 SD 내용부터 확인한다.

## 리스크

- USB-BT 클럭 드리프트. 가젯 클럭과 BT 클럭이 다른 소스라 비동기 리샘플링이
  필요하고, UAC2 async 피드백이 관건이며 커널 버전을 탄다.
- "USB0 가젯 동작 확인됨"은 ROM DFU 열거까지다. 리눅스 위에서 configfs로 UAC2 가젯을
  올려 iPhone이 DAC로 인식하는 것은 3단계에서 처음 확인한다.

## 선례

같은 신호 경로(UAC2 입력에서 LDAC 송출)를 MCU 하나로 구현한 공개 프로젝트가 있다.
`wasdwasd0105/PicoW-usb2bt-audio`. Pico 2 W(RP2350)에서 SBC, AAC, LDAC, LHDC를
돌리며 RP2040은 지원하지 않는다고 명시한다. 이 계획은 리눅스 경로를 택했지만,
3단계에서 막히면 PB2 대신 RP2350 보드로 내려가는 대안이 있다는 뜻이다.
LDAC 인코더가 M33 급에서 돈다는 근거이기도 하다.

## 2차 계획

Raspberry Pi OS로 완주한 뒤 Buildroot 최소 이미지로 재제작한다(부팅 수 초,
read-only 루트).

## 확인 안 된 것

- 맥북 대비 iPhone 음질 차이의 원인. 맥에서 option+BT 클릭으로 협상 코덱을
  확인하면 판정된다.
- 보유 중인 C타입 동글 모델.
