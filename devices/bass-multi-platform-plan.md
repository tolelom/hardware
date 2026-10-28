# 멀티 이펙터 하드웨어 플랫폼 구현 계획 (회로도 단계)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 하드웨어 플랫폼 설계서의 패널(F4, 1590R1)을 기구 도구로 검증하고 CNC 도면을 내며, Seed3 핀 배정을 코드로 고정하고, 플랫폼 회로(계승 블록 A~F + 새 블록)를 BLOCKS 형식으로 만들어 회로도·넷리스트·SPICE까지 생성한다.

**Architecture:** `enclosure.py`는 엔진으로 두고 설계 데이터를 `designs/`로 분리한다(1호 1590C, 플랫폼 1590R1). `pinmap.py`가 Seed3 핀 능력 표와 배정을 가진다. `platform_blocks.py`가 `gen_schematic.BLOCKS`의 A~F를 가져와 새 블록을 더하고, `gen_schematic.render`는 블록 목록과 출력 이름을 인자로 받는다. 1호 회로도 출력은 바이트 단위로 그대로다.

**Tech Stack:** Python 3 표준 라이브러리, unittest, ngspice 47, KiCad 심볼 라이브러리 9.0.7 파일(GitLab raw)

**Spec:** [bass-multi-platform-design.md](bass-multi-platform-design.md)

## Global Constraints

- 파이썬은 표준 라이브러리만. DXF는 직접 쓴다(ASCII R12)
- 테스트는 저장소 루트에서 `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware`
- 1호 BLOCKS(`gen_schematic.BLOCKS`)와 그 출력(`bass-compressor.kicad_sch`, `netlist.md`, `spice/generated/`)은 바꾸지 않는다
- 부품 값·핀 번호는 데이터시트나 KiCad 심볼 파일로 확인한 것만 넣는다. 확인 못 한 것은 설계서·회로 설계 문서에 "확인 필요"로 남긴다
- 문서는 기존 문체("~다", 표), em dash 금지. 커밋은 `[Feat]`·`[Docs]`·`[Test]`·`[Refactor]` 머리말
- 커밋은 경로를 지정해 넣는다(`git add -A` 금지)
- PCB 레이아웃, 풋프린트 배정, BOM·LCSC 품번은 이 계획 범위 밖

## Review Focus

- 1호 회로도 재생성이 바이트 단위로 달라짐: 리팩터 뒤 기존 출력과 같아야 한다 (Task 4)
- 같은 Seed3 핀을 두 기능에 배정하거나 ADC가 아닌 핀에 아날로그 입력을 배정: 핀 검사가 실패해야 한다 (Task 3)
- 사각형 부품(화면 창)과 원형 부품의 간섭, 사각형의 가장자리 여유: 원-원과 같은 기준으로 판정해야 한다 (Task 1)
- 뒷면 단자 몸체가 안쪽 기판 스택과 겹침: 3D 간섭으로 잡혀야 한다 (Task 1)
- DXF가 CAM 도구에서 열리지 않음: 최소 R12 구조(HEADER 없이 ENTITIES만, CIRCLE·LWPOLYLINE 대신 LINE)를 지키고 파서로 되읽어 확인 (Task 2)

---

### Task 1: 기구 엔진에 사각형 부품과 설계 데이터 분리

**Files:**
- Modify: `effector/hardware/enclosure.py`
- Create: `effector/hardware/designs/__init__.py`, `effector/hardware/designs/compressor_1590c.py`, `effector/hardware/designs/platform_1590r1.py`
- Modify: `effector/hardware/tests/test_enclosure.py`

**Interfaces:**
- Produces:
  - `PartSpec`에 `cutout: tuple[Dimension, Dimension] | None = None`(사각 타공 폭·높이), `outside_rect: tuple[Dimension, Dimension] | None = None`(바깥 점유 사각형) 추가. 원형은 기존 `hole_d`, `outside_d`
  - `Design(name, case, parts, placements, internal)` 데이터클래스, `designs.compressor_1590c.DESIGN`, `designs.platform_1590r1.DESIGN`
  - `main(argv)`에 `--design {compressor_1590c, platform_1590r1}` (기본 compressor_1590c로 기존 동작 유지)
  - 간섭 판정은 원-원, 원-사각, 사각-사각 모두 "가장 가까운 거리 - 여유"로 계산

플랫폼 데이터(대표값은 출처 "대표값", 확인값은 출처를 적는다):
- 케이스 1590R1: 192 × 111.25 × 60.76mm, 벽 2.03mm(판매점 DigiKey), 가장자리 여유 미측정
- 화면: 타공 75.4 × 51mm(보이는 영역 74.4 × 50mm + 여유 0.5mm씩, Orient·EA 공통), 바깥 점유 사각형 82.9 × 54.7mm(모듈 외형, 패널 아래라 바깥 간섭에는 쓰지 않고 안쪽 상자로만), 패널 아래 높이 7.74mm(Orient 두께)
- 인코더: 타공 7.5mm, 바깥 점유 15mm(노브), 몸체 12.5 × 13.4mm, 패널 아래 높이 미측정(EC11 대표 20mm로 초안)
- NAV: 바깥 점유 20mm
- 풋스위치·LED: 기존 값
- 뒷면 단자 7개: 1/4" TRS 타공 9.5mm·바깥 14mm·몸체 16 × 16 × 30mm(대표값), 3.5mm TRS 타공 6mm·바깥 8mm·몸체 8 × 10 × 15mm(대표값), USB-C 사각 타공 9.5 × 3.8mm·몸체 미측정, DC 타공 12mm
- 배치(F4, 면 좌표 = 최종 시안 그림 좌표에서 x − 96, 55.625 − y): 화면 타공 중심 (46.55, 20.28), 인코더 (−54, 34.63)(−31, 34.63)(−8, 34.63)(−54, 11.63)(−31, 11.63)(−8, 11.63), NAV (−79, 33.63), LED y −18.38, 풋스위치 y −34.38, x −60·0·60. 뒷면 단자는 바깥에서 본 오른쪽이 +x라 위에서 본 순서(OUT, IN, EXP, MIDI OUT, MIDI IN, USB-C, DC)가 x 76, 52, 26, 3, −17, −44, −72, y 0

- [ ] **Step 1: 실패하는 테스트 작성** (`test_enclosure.py`에 추가)

```python
class RectTest(unittest.TestCase):
    def rect_spec(self, name: str, w: float, h: float) -> en.PartSpec:
        return en.PartSpec(name, M(1, "t"), M(1, "t"), M(w, "t"), M(h, "t"), M(5, "t"),
                           cutout=(M(w, "t"), M(h, "t")), outside_rect=(M(w, "t"), M(h, "t")))

    def test_rect_circle_gap(self) -> None:
        c = case()
        parts = {"scr": self.rect_spec("scr", 40, 20), "knob": spec("knob", 10)}
        near = [en.Placement("s", "scr", "top", 0, 0), en.Placement("k", "knob", "top", 24, 0)]
        far = [en.Placement("s", "scr", "top", 0, 0), en.Placement("k", "knob", "top", 26, 0)]
        values = values_of(c, parts, [])
        bad = [f for f in en.check(c, parts, near, [], values) if f.check == "패널 바깥 간섭" and not f.ok]
        good = [f for f in en.check(c, parts, far, [], values) if f.check == "패널 바깥 간섭" and not f.ok]
        self.assertTrue(bad)
        self.assertFalse(good)

    def test_rect_edge_margin(self) -> None:
        c = case()
        parts = {"scr": self.rect_spec("scr", 40, 20)}
        values = values_of(c, parts, [])
        inside = en.check(c, parts, [en.Placement("s", "scr", "top", 47.5 - 5 - 20, 0)], [], values)
        outside = en.check(c, parts, [en.Placement("s", "scr", "top", 47.5 - 5 - 19.9, 0)], [], values)
        self.assertTrue(all(f.ok for f in inside if f.check == "가장자리 여유"))
        self.assertFalse(all(f.ok for f in outside if f.check == "가장자리 여유"))


class DesignTest(unittest.TestCase):
    def test_platform_design_draft_has_no_failures(self) -> None:
        from designs import platform_1590r1 as d
        values, missing = en.resolve(d.DESIGN.case, d.DESIGN.parts, [], draft=True)
        findings = en.check(d.DESIGN.case, d.DESIGN.parts, d.DESIGN.placements, d.DESIGN.internal(values), values)
        failed = [f for f in findings if not f.ok]
        self.assertEqual(failed, [], failed)

    def test_back_jack_body_collides_with_tall_board_part(self) -> None:
        from designs import platform_1590r1 as d
        values, _ = en.resolve(d.DESIGN.case, d.DESIGN.parts, [], draft=True)
        tall = en.Box("시험용 키 큰 부품", -10, 10, 30, 50, 2, 40)
        findings = en.check(d.DESIGN.case, d.DESIGN.parts, d.DESIGN.placements, [tall], values)
        self.assertTrue(any(not f.ok and "시험용" in f.detail for f in findings if f.check == "패널 안쪽 간섭"))

    def test_compressor_design_output_unchanged(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                en.main(["--draft", "--out", directory])
            for name in ("top.svg", "report.md"):
                with self.subTest(name=name):
                    self.assertEqual((Path(directory) / name).read_text(encoding="utf-8"),
                                     (Path(en.__file__).parent / "enclosure" / name).read_text(encoding="utf-8"))
```

- [ ] **Step 2: 실패 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -p "test_enclosure.py" -v`
Expected: `TypeError`(cutout 인자 없음), `ModuleNotFoundError: designs`

- [ ] **Step 3: 구현**

1. `PartSpec`에 두 필드를 기본값 `None`으로 추가하고 `resolve`가 두 필드의 Dimension도 `<part>.cutout_w` 등 키로 풀게 한다.
2. 거리 함수 `_gap(a, b, values)`를 둔다. 원은 (중심, 반지름), 사각은 (중심, 반폭, 반높이). 원-원은 중심 거리 − 반지름 합, 원-사각은 사각형까지의 점 거리 − 반지름, 사각-사각은 축 방향 간격의 유클리드 결합(겹치면 음수).
3. 가장자리 여유는 사각형이면 `반폭`, `반높이`를 각각 써서 `min(width/2 − margin − |x| − 반폭, height/2 − margin − |y| − 반높이)`.
4. `body_box`는 `outside_rect`가 있으면 그 크기를 몸체 단면으로 쓴다.
5. 기존 상수(`CASE`, `PARTS`, `PLACEMENTS`, `INTERNAL_SPECS`, `INTERNAL_POSITION`, `internal_boxes`)를 `designs/compressor_1590c.py`로 옮기고 `enclosure.py`에서는 `from designs.compressor_1590c import ...`로 다시 내보내 기존 테스트 이름을 유지한다.
6. `designs/platform_1590r1.py`에 위 데이터와 `DESIGN.internal(values)`(Seed3 스택 상자를 화면 아래 x 40..80, y −15..30, z 바닥~+20로 초안)를 둔다.
7. `main`에 `--design` 인자를 더하고 출력 폴더 기본값을 `enclosure/<design>`로 하되, 기본 디자인은 기존 `enclosure/` 그대로 쓴다.
8. `face_svg`는 사각 타공을 `<rect>`로 그린다.

- [ ] **Step 4: 통과 확인**

Run: `python3 -m unittest discover -s effector/hardware/tests -t effector/hardware -v` 그리고 `python3 effector/hardware/enclosure.py --draft --design platform_1590r1`
Expected: 테스트 OK, 플랫폼 초안 검사 실패 0건. 실패가 나면 배치 좌표를 고치고 고친 값을 설계서 3절에 반영한다

- [ ] **Step 5: 커밋**

```bash
git add effector/hardware/enclosure.py effector/hardware/designs/ effector/hardware/tests/test_enclosure.py effector/hardware/enclosure/
git commit -m "[Feat] 기구 엔진 사각형 부품, 설계 데이터 분리, 1590R1 플랫폼 배치"
```

---

### Task 2: CNC용 DXF 출력

**Files:**
- Create: `effector/hardware/dxf.py`
- Modify: `effector/hardware/enclosure.py` (`--dxf` 출력)
- Create: `effector/hardware/tests/test_dxf.py`

**Interfaces:**
- Produces: `dxf.document(entities: list[tuple]) -> str`, 엔티티 `("circle", cx, cy, r, layer)`, `("rect", cx, cy, w, h, layer)`(LINE 4개로 쓴다), `("text", x, y, h, s, layer)`; `dxf.parse(text) -> list[tuple]`(시험용 되읽기). 단위 mm, 면 중심이 원점, 레이어 `CUT`(타공), `OUTLINE`(면 외곽), `NOTE`(좌표 글자)

- [ ] **Step 1: 실패하는 테스트**

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import dxf  # noqa: E402


class DxfTest(unittest.TestCase):
    def test_round_trip(self) -> None:
        text = dxf.document([("circle", 10, -5, 3.75, "CUT"), ("rect", 0, 0, 20, 10, "OUTLINE")])
        self.assertTrue(text.startswith("0\nSECTION\n2\nENTITIES\n"))
        self.assertTrue(text.rstrip().endswith("0\nEOF"))
        entities = dxf.parse(text)
        self.assertIn(("CIRCLE", "CUT", (10.0, -5.0, 3.75)), entities)
        lines = [e for e in entities if e[0] == "LINE"]
        self.assertEqual(len(lines), 4)

    def test_number_format_has_no_exponent(self) -> None:
        text = dxf.document([("circle", 0.00001, 123456.5, 0.5, "CUT")])
        self.assertNotIn("e", text.lower().replace("section", "").replace("entities", "").replace("eof", "").replace("circle", "").replace("endsec", ""))
```

- [ ] **Step 2: 실패 확인** — `ModuleNotFoundError: dxf`
- [ ] **Step 3: 구현** — 그룹 코드 0/8/10/20/40(원), 0/8/10/20/11/21(선), 0/8/10/20/40/1(글자). 숫자는 `f"{v:.4f}"`. `parse`는 쌍으로 읽어 `("CIRCLE", layer, (x, y, r))`, `("LINE", layer, (x1, y1, x2, y2))`, `("TEXT", layer, (x, y, h, s))`를 돌려준다. `enclosure.py main`에 `--dxf`가 있으면 면마다 `<face>.dxf`를 함께 쓴다.
- [ ] **Step 4: 통과 확인** — 전체 테스트 OK, `python3 effector/hardware/enclosure.py --draft --design platform_1590r1 --dxf`로 `top.dxf`, `back.dxf` 생성
- [ ] **Step 5: 커밋** — `[Feat] 기구 도구 CNC용 DXF(R12 ENTITIES) 출력`

---

### Task 3: Seed3 핀 배정 모듈

**Files:**
- Create: `effector/hardware/pinmap.py`
- Create: `effector/hardware/tests/test_pinmap.py`

**Interfaces:**
- Produces:
  - `SEED3_PINS: dict[str, Pin]`(D0~D30), `Pin(name, physical: int, adc: str | None, functions: frozenset[str], reserved: str | None)`
  - `PLATFORM: dict[str, str]`(기능 이름 → 핀 이름)
  - `check(assignment: dict[str, str]) -> list[str]`(문제 목록). 규칙: 핀 중복 금지, `*_ADC` 기능은 ADC 핀, `SPI1_*`·`I2C1_*`·`USART1_*`·`SDMMC1_*`은 그 기능을 가진 핀, `reserved` 핀(D29·D30 USB) 사용 금지
  - `net(function) -> str` 넷 이름 `SEED_<핀>_<기능>`

핀 표 출처: libDaisy `src/daisy_seed.h`(D0~D30 포트, A0~A11 별칭), Seed3 데이터시트 표 2(물리 핀 번호, 대체 기능). 대체 기능은 데이터시트 표에서 옮기고, 표에 없으면 STM32H750 데이터시트 AF 표로 확인한다. 옮긴 값마다 `# 데이터시트 표 2` 같은 주석을 단다.

플랫폼 배정(설계서 5.3절): K1_DRIVE D0, SDMMC1_D3 D1, SDMMC1_D2 D2, SDMMC1_D1 D3, SDMMC1_D0 D4, SDMMC1_CMD D5, SDMMC1_CK D6, SPI1_CS D7, SPI1_SCK D8, SPI1_MISO D9(미사용 예약), SPI1_MOSI D10, I2C1_SCL D11, I2C1_SDA D12, USART1_TX D13(MIDI OUT), USART1_RX D14(MIDI IN), EXP_ADC D15(A0), VMON_ADC D20(A5), 나머지(화면 DC·RST·백라이트 PWM, MCP 인터럽트 2, 풋스위치 3, LED 3)는 D16~D19, D21~D28에서 고른다. 백라이트 PWM은 타이머 채널이 있는 핀이어야 한다(데이터시트로 확인).

- [ ] **Step 1: 실패하는 테스트**

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import pinmap  # noqa: E402


class PinmapTest(unittest.TestCase):
    def test_table_shape(self) -> None:
        self.assertEqual(len(pinmap.SEED3_PINS), 31)
        self.assertEqual(pinmap.SEED3_PINS["D15"].adc, "A0")
        self.assertEqual(pinmap.SEED3_PINS["D28"].adc, "A11")
        self.assertIsNotNone(pinmap.SEED3_PINS["D29"].reserved)

    def test_platform_assignment_is_clean(self) -> None:
        self.assertEqual(pinmap.check(pinmap.PLATFORM), [])

    def test_detects_duplicate_and_wrong_capability(self) -> None:
        bad = dict(pinmap.PLATFORM)
        bad["FS1"] = bad["K1_DRIVE"]
        bad["EXP_ADC"] = "D0"
        bad["SPI1_SCK"] = "D16"
        bad["USB_SPARE"] = "D29"
        problems = " ".join(pinmap.check(bad))
        for word in ("중복", "ADC", "SPI1_SCK", "D29"):
            with self.subTest(word=word):
                self.assertIn(word, problems)

    def test_net_name(self) -> None:
        self.assertEqual(pinmap.net("EXP_ADC"), "SEED_D15_EXP_ADC")
```

- [ ] **Step 2: 실패 확인** — `ModuleNotFoundError: pinmap`
- [ ] **Step 3: 구현** — 데이터시트 표 2를 읽어 표를 채운다(물리 핀 번호는 기존 BLOCKS H 블록의 J4 넷과 대조). 배정의 GPIO 핀은 대체 기능 충돌이 없는 것으로 고르고 PWM 핀을 확인한다
- [ ] **Step 4: 통과 확인** — 전체 테스트 OK
- [ ] **Step 5: 커밋** — `[Feat] Seed3 핀 능력 표와 플랫폼 배정 검사`

---

### Task 4: 회로도 생성기를 블록 목록 인자로

**Files:**
- Modify: `effector/hardware/gen_schematic.py`
- Modify: `effector/hardware/tests/test_spice_models.py` (회로도 재생성 비교)

**Interfaces:**
- Produces: `render(library, out, blocks=BLOCKS, name="bass-compressor", title="베이스 컴프레서 넷리스트")`, `check(netlist, blocks=BLOCKS)`. 기본값이면 기존과 바이트 단위로 같은 출력

- [ ] **Step 1: 실패하는 테스트** (`test_spice_models.py`에 추가, 심볼 라이브러리 폴더가 환경변수 `KICAD_SYMBOL_DIR`에 있을 때만)

```python
import os


@unittest.skipUnless(os.environ.get("KICAD_SYMBOL_DIR"), "KICAD_SYMBOL_DIR 없음(KiCad 9.0.7 심볼 11개 이상)")
class SchematicRegenerationTest(unittest.TestCase):
    def test_default_blocks_reproduce_committed_files(self) -> None:
        import gen_schematic as gs
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                gs.render(gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"])), Path(directory))
            for name in ("bass-compressor.kicad_sch", "netlist.md"):
                with self.subTest(name=name):
                    self.assertEqual((Path(directory) / name).read_bytes(), (gs.HERE / name).read_bytes())

    def test_custom_name_and_blocks(self) -> None:
        import gen_schematic as gs
        with tempfile.TemporaryDirectory() as directory:
            with contextlib.redirect_stdout(io.StringIO()):
                gs.render(gs.Library(Path(os.environ["KICAD_SYMBOL_DIR"])), Path(directory),
                          blocks=gs.BLOCKS[:2], name="part", title="부분")
            self.assertTrue((Path(directory) / "part.kicad_sch").exists())
            self.assertTrue((Path(directory) / "part-netlist.md").exists())
```

(테스트 파일 맨 위에 `import contextlib, io` 추가)

- [ ] **Step 2: 실패 확인** — `KICAD_SYMBOL_DIR=<스크래치패드>/kicad-symbols-9.0.7`로 실행해 두 번째 테스트가 `TypeError`
- [ ] **Step 3: 구현** — `render`와 `check`, `write_netlist`가 인자를 받게 하고, 출력 파일 이름은 기본 `bass-compressor.kicad_sch`·`netlist.md`, 그 밖에는 `<name>.kicad_sch`·`<name>-netlist.md`. `uid`의 PROJECT는 이름별로 `PROJECT if name == "bass-compressor" else name`. `HERE = Path(__file__).resolve().parent` 상수를 둔다
- [ ] **Step 4: 통과 확인** — 두 테스트 OK(기본 출력 바이트 동일), 전체 테스트 OK
- [ ] **Step 5: 커밋** — `[Refactor] 회로도 생성기가 블록 목록과 출력 이름을 인자로 받음`

---

### Task 5: 플랫폼 회로 블록

**Files:**
- Create: `effector/hardware/platform_blocks.py`
- Modify: `effector/hardware/gen_spice.py` (새 심볼을 SKIPPED 또는 매핑)
- Create: `effector/hardware/tests/test_platform_blocks.py`
- Create: 생성물 `effector/hardware/platform.kicad_sch`, `platform-netlist.md`, `spice/generated/platform_*.cir`

**Interfaces:**
- Consumes: `gen_schematic.BLOCKS`(A~F), `pinmap.PLATFORM`, `pinmap.net`
- Produces: `PLATFORM_BLOCKS: list[Block]`, 각 블록 제목 앞에 기판 표시 `[메인]` 또는 `[조작부]`; `BOARD_LINK`(두 기판 사이 커넥터의 핀별 넷 목록)

블록 구성(설계서 4절·5.2절). 부품 값과 핀 번호는 Step 1에서 데이터시트·심볼로 확인하고 확인한 출처를 Part `note`에 적는다.

| 블록 | 기판 | 내용 |
|---|---|---|
| A~F | 메인 | 계승. 단 F의 IN_TIP·OUT_TIP은 뒷면 잭으로 이름 그대로 |
| L 보조 3.3V | 메인 | V9 → 강압형 3.3V(500mA 이상) → 페라이트 + 10µF → 3V3_AUX. 부품은 입력 9~12.7V(과전압 차단 임계 이하)를 견디고 SOT-23 급 이상 손납땜 가능한 것 중 KiCad 심볼이 있는 것으로 고른다(후보 TI TPS562201, 데이터시트 기준 외부 부품값) |
| H2 Seed3 소켓 | 메인 | J4 핀을 `pinmap.PLATFORM`에서 넷으로. 1호 H 블록과 같은 물리 핀 번호 |
| M MIDI | 메인 | IN: TRS 타입 A 3.5mm, 220Ω, 1N4148, 광커플러(Seed3 Pedal Dev Kit와 같은 H11L1 계열, 3.3V 동작 확인), 풀업 → USART1_RX. OUT: MMA CA-033의 3.3V 저항값으로 USART1_TX → TRS 타입 A |
| N EXP | 메인 | TRS 1/4": 링에 3V3A를 1k 직렬로, 팁 와이퍼 → 1k + 100nF → EXP_ADC, 팁 100k 풀다운(뺐을 때 0) |
| O microSD | 메인 | 소켓(KiCad Connector `Micro_SD_Card`), CMD·D0~D3 47k 풀업 3V3_AUX, CLK 직렬 33Ω, 디커플링 100nF + 10µF |
| P 스테레오 출력 자리 | 메인 | 블록 E와 같은 값의 출력단 한 채널(U3 OPA2992 A), Audio Out 2에서 OUT 잭 링으로. 모든 부품 note에 "DNP(자리만)" |
| Q 기판 연결 | 메인·조작부 | 2×10 헤더: V3V3_AUX, GND×3, SPI(SCK, MOSI, CS, DC, RST), BL_PWM, I2C(SCL, SDA), INT_A, INT_B, FS1~3, LED1~3 |
| G2 조작부 입력 | 조작부 | MCP23017 ×2(I2C 주소 0x20·0x21, RESET 10k 풀업, 100nF), I2C 풀업 4.7k ×2, 인코더 7개(EC11: A·B는 10k 풀업 + 10nF, C는 GND, 누름은 MCP 내부 풀업), 풋스위치 3개(10k 풀업 + 100nF, 1호 R22·C15와 같은 값), LED 3개(470Ω) |
| K 화면 | 조작부 | 화면 모듈 헤더(VCC 3V3_AUX, GND, CS, DC, RST, SCK, MOSI), 백라이트 저측 N-MOSFET(게이트 100Ω, 풀다운 100k), 모듈 백라이트 전류 제한 저항이 모듈에 있는지 Step 1에서 확인 |

- [ ] **Step 1: 데이터 확인**
  1. 필요한 KiCad 심볼 파일을 같은 GitLab raw 경로로 더 받는다: `Interface_Expansion`, `Isolator`, `Regulator_Switching`, `Connector_Card`(없으면 `Connector`). 각 심볼의 핀 번호와 이름을 출력하는 짧은 스크립트로 확인하고 결과를 이 Task의 커밋 메시지 본문에 요약한다
  2. 강압 레귤레이터, H11L1, MCP23017, MIDI CA-033 값, 화면 모듈 핀 배치(Orient AFK320480A0)를 데이터시트·규격 문서에서 읽어 note에 출처를 적는다. 찾지 못한 값은 회로 설계 문서에 "확인 필요"로 남긴다
- [ ] **Step 2: 실패하는 테스트**

```python
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gen_spice  # noqa: E402
import pinmap  # noqa: E402
import platform_blocks as pb  # noqa: E402


class PlatformBlocksTest(unittest.TestCase):
    def all_parts(self):
        return [p for b in pb.PLATFORM_BLOCKS for p in b.parts]

    def test_every_block_names_its_board(self) -> None:
        for block in pb.PLATFORM_BLOCKS:
            with self.subTest(block=block.title):
                self.assertTrue(block.title.startswith(("[메인]", "[조작부]")))

    def test_inherits_analog_blocks_unchanged(self) -> None:
        from gen_schematic import BLOCKS
        inherited = {p.ref: p for b in BLOCKS[:6] for p in b.parts}
        platform = {p.ref: p for p in self.all_parts()}
        for ref, part in inherited.items():
            with self.subTest(ref=ref):
                self.assertEqual(platform[ref].value, part.value)
                self.assertEqual(platform[ref].nets, part.nets)

    def test_seed_socket_matches_pinmap(self) -> None:
        socket = next(p for p in self.all_parts() if p.ref == "J4")
        for function, pin in pinmap.PLATFORM.items():
            physical = str(pinmap.SEED3_PINS[pin].physical)
            with self.subTest(function=function):
                self.assertEqual(socket.nets[physical], pinmap.net(function))

    def test_board_link_nets_exist_on_both_boards(self) -> None:
        nets_by_board = {"[메인]": set(), "[조작부]": set()}
        for block in pb.PLATFORM_BLOCKS:
            board = "[메인]" if block.title.startswith("[메인]") else "[조작부]"
            for p in block.parts:
                nets_by_board[board] |= set(p.nets.values())
        for net in pb.BOARD_LINK:
            with self.subTest(net=net):
                self.assertIn(net, nets_by_board["[메인]"])
                self.assertIn(net, nets_by_board["[조작부]"])

    def test_spice_mapping_covers_platform(self) -> None:
        power = gen_spice.opamp_power(pb.PLATFORM_BLOCKS)
        for p in self.all_parts():
            gen_spice.part_lines(p, power)
```

- [ ] **Step 3: 실패 확인** — `ModuleNotFoundError: platform_blocks`
- [ ] **Step 4: 구현** — `platform_blocks.py`에 블록을 쓴다. 지정자는 1호와 겹치지 않게 블록별로 정한다(예: L 블록 U10·L1·C30~, M 블록 U11·J10·R40~, G2 블록 U20·U21·SW10~·R60~). `gen_spice.SKIPPED`에 디지털 IC·커넥터·인코더 심볼을 더하고, 아날로그 시험이 필요한 N-MOSFET은 `NMOSQ` 모델로 매핑한다. `gen_spice.generate`에 블록 목록·접두사 인자를 더해 `platform_<문자>.cir`를 쓴다. 회로도는 `render(..., blocks=PLATFORM_BLOCKS, name="platform", title="멀티 이펙터 플랫폼 넷리스트")`
- [ ] **Step 5: 통과 확인** — 전체 테스트 OK, 회로도 `check()` 통과(넷마다 핀 2개 이상, 지정자 중복 없음)
- [ ] **Step 6: 커밋** — `[Feat] 멀티 이펙터 플랫폼 회로 블록(계승 A~F와 새 블록), 회로도·SPICE 생성`

---

### Task 6: 결과 반영

**Files:**
- Modify: `devices/bass-multi-platform-design.md` (3절 확정 좌표, 5.3절 확정 핀, 5.4절 전류, 7절)
- Create: `devices/bass-multi-platform-schematic.md` (새 블록 값과 근거, 확인 필요 목록)
- Modify: `effector/README.md` (`--design`, `--dxf`, 플랫폼 회로도 생성 명령)

- [ ] **Step 1:** 전체 테스트와 `enclosure.py --draft --design platform_1590r1 --dxf`, 플랫폼 회로도 생성을 돌린다
- [ ] **Step 2:** 설계서 3·5절을 확정 값으로 고치고, 새 회로 설계 문서에 블록별 부품 표(값, 근거, 출처)와 "확인 필요" 목록을 쓴다
- [ ] **Step 3:** 커밋 `[Docs] 멀티 이펙터 플랫폼 회로도 단계 결과 반영`

## 실행 기록

계획과 다르게 구현한 것은 여기에 적는다.
