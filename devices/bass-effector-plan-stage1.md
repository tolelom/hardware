# 베이스 컴프레서 1단계 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Seed3와 DigiKey 주문 품번을 확정하고, Daisy Seed3로 디지털 경로(컴프레서 펌웨어)를 만들어 설계서 1단계 측정(M2, T5, T7, T8, T9, T15)을 통과시킨다.

**Architecture:** 신호 처리 로직과 시리얼 콘솔은 플랫폼 헤더가 없는 코어 라이브러리(`effector/lib/effector_core`)에 두고 PC에서 CMake와 g++로 테스트한다. Seed3 펌웨어(`effector/src/main.cpp`)는 libDaisy 오디오 콜백이 코어를 부르고 USB CDC를 콘솔에 잇는 얇은 층이고, 1단계에는 노브 대신 시리얼 명령으로 파라미터를 넣는다.

**Tech Stack:** C++17, CMake 3.16 이상, g++ (네이티브 테스트), libDaisy(서브모듈, 커밋 265aae3)와 arm-none-eabi-gcc, make, dfu-util(Seed3 빌드·업로드), Audacity(측정)

**Spec:** [bass-effector-design.md](bass-effector-design.md), 단계표는 [bass-effector.md](bass-effector.md)

## Global Constraints

- 설계 기준은 설계서다. 수치와 규칙은 설계서 절 번호로 인용한다
- 1단계 범위: 계획서 단계 1(M2, T5, T7, T8, T9, T15). 아날로그 입출력단, K1 릴레이, 노브, LED 하드웨어는 넣지 않는다
- 샘플레이트 48000Hz, 오디오 블록 48샘플. libDaisy 기본값이고 44.1kHz는 없다
- `lib/effector_core`는 libDaisy·STM32 헤더를 포함하지 않는다
- 네이티브 빌드: C++17, `-Wall -Wextra -Werror`
- 변수·함수 이름은 줄여 쓰지 않는다. 단위(`Db`, `Hz`, `Seconds`)와 id는 허용
- 커밋 메시지는 `[Tag] 한국어 요약`, Co-Authored-By 트레일러 없음, 푸시하지 않는다
- md 문서는 em dash·en dash·스마트 따옴표를 쓰지 않는다
- Seed3 빌드는 `effector/`에서 `make`, 업로드는 `make program-dfu`. 서브모듈 받기와 libDaisy 빌드는 `effector/README.md`. 집 Windows의 툴체인과 Audacity 설치는 사용자가 한다
- Seed3 Audio In에 1Vrms(+2.2dBu)를 넘는 신호를 넣지 않는다. M2 교정 전에는 스칼렛 출력 레벨을 -20dBFS 이하로 둔다
- 초기값 상수(드라이브 최대 30dB, 미터 경계, 노브 범위)는 M1, T16 결과로 조정한다

## 선행 조건

- M1(보유 베이스 피크) 측정은 사용자가 한다. 결과는 Task 9의 측정 기록 파일에 함께 적는다

## File Structure

| 경로 | 책임 |
|---|---|
| `effector/CMakeLists.txt` | 네이티브 테스트와 도구 빌드 |
| `effector/Makefile` | Seed3 빌드와 DFU 업로드. libDaisy core Makefile 포함 |
| `effector/lib/libDaisy/` | libDaisy 서브모듈(고정 커밋) |
| `effector/README.md` | 빌드·업로드·테스트 명령 |
| `effector/lib/effector_core/src/dc_blocker.{h,cpp}` | 1차 DC 제거 필터 |
| `effector/lib/effector_core/src/compressor.{h,cpp}` | RMS 검출 컴프레서, 사이드체인 HPF |
| `effector/lib/effector_core/src/meter.{h,cpp}` | 감쇠 미터 8칸과 클립 점멸 |
| `effector/lib/effector_core/src/parameter_mapping.{h,cpp}` | 노브 값 → 파라미터 |
| `effector/test/check.h`, `test_main.cpp`, `signal_helpers.h` | 테스트 하네스와 신호 생성 |
| `effector/test/test_*.cpp` | 코어 단위 테스트 |
| `effector/test/reference/openaudio_compressor.h` | T9 대조군(OpenAudio 식 이식) |
| `effector/tools/static_gain.cpp` | 실기기 T9 대조값 계산 |
| `effector/lib/effector_core/src/effector_processor.{h,cpp}` | DC 제거, 프리게인, 컴프레서, Blend, Output을 한 블록에서 |
| `effector/lib/effector_core/src/serial_console.{h,cpp}` | 1단계 시리얼 명령. `ConsoleHost`로 플랫폼을 분리 |
| `effector/src/main.cpp` | Seed3 층. libDaisy 오디오 콜백, USB CDC 링 버퍼, 콘솔 호스트 |
| `effector/measurements/stage1.md` | 1단계 측정 기록 |

---

### Task 1: Seed3와 DigiKey 주문 품번 확정

**Files:**
- Modify: `devices/bass-effector.md` (부품 절 표)

**Interfaces:**
- Consumes: 설계서 3.5절(전원 보호), 4.1절(블록도), 4.3절(Seed3), 5.4절(릴레이)
- Produces: 계획서 부품표의 daisy.audio·DigiKey 행 품번과 수량. 사용자가 이 표로 주문한다

- [ ] **Step 1: 보유품 확인**

`inventory.md` 2절(전원)과 7절(배선)에서 USB-C 데이터 케이블과 점퍼선·브레드보드 보유를 확인한다. USB-C 케이블 보유가 "확인 필요"면 사용자에게 묻고, 없으면 아래 표에 케이블 행을 추가한다.

- [x] **Step 1a: Seed3 주문**

daisy.audio에서 Seed3 2개를 주문했다(2026-09-15). 핀 헤더 동봉 여부는 상품 페이지에 없어 아래 표의 헤더를 동봉 여부와 상관없이 산다. 배송이 가장 오래 걸려 DigiKey보다 먼저 넣었다.

- [x] **Step 2: 후보 품번 재고·사양 확인**

2026-09-15에 아래 표의 모든 행이 DigiKey 코리아에서 재고 있음, 활성으로 확인됐고 같은 날 이 표대로 장바구니를 채웠다(백오더 없음, 무료 배송). 40핀 헤더는 장바구니에 두 줄(1개, 2개)로 들어 있다. 같은 날 DigiKey 부품을 2대분으로 늘렸다. 두 번째 세트는 1호 제작 중 망가진 부품의 예비로 먼저 쓰고, 남으면 2호(Seed3 두 번째 모듈)에 쓴다. 한 번 망가져 재주문하면 ₩60,000 미만 배송비 ₩20,000이 부품값보다 크기 때문이다. 표의 수량은 2대분이다.

각 행을 `https://www.digikey.kr/ko/products/result?keywords=<품번>`으로 검색해 상품 상세 페이지에서 재고와 수량별 단가를 기록한다. 기준을 못 맞추면 같은 기준을 만족하는 다른 품번으로 바꾸고 이유를 적는다.

| 기능 | 후보 품번 | 수량 | 기준 |
|---|---|---|---|
| 소켓 | Sullins PPPC201LFBN-RC | 4 | 2.54mm 20핀 암 소켓, 스루홀. Seed3를 기판에 꽂는다. 한 대에 2개 |
| 헤더 | Chip Quik HDR100IMP40M-G-V-TH | 3 | 2.54mm 스트레이트 40핀. 1개는 변환 기판 2장의 다리용(16핀), 2개는 Seed3 2개용이다. BOB-13655와 Seed3 모두 헤더 동봉 여부가 상품 페이지에 없어 동봉을 전제하지 않는다 |
| op-amp | TI OPA2992IDR | 2 | SOIC-8. 한 대에 1개 |
| SOIC 변환 기판 | SparkFun BOB-13655 | 1팩 | SOIC-8을 DIP-8 핀 간격으로. 4개입이라 2대분에 2장이 남는다 |
| 차지 펌프 | TI LT1054CP | 2 | PDIP-8. 한 대에 1개 |
| 릴레이 | Panasonic TQ2-9V | 2 | 논래칭 DPDT, 9V 코일. 한 대에 1개 |
| P-MOSFET | Infineon IRF9540NPBF | 4 | VGS 정격 ±20V 이상, VDS 절댓값 30V 이상, TO-220. 한 대에 백투백 2개 |
| 12V 제너 | onsemi 1N4742A | 2 | 12V, 1W, 스루홀 |
| 15V 제너 | onsemi 1N4744A | 2 | 15V, 1W, 스루홀. Q1·Q2 게이트 보호 D14. 한 대에 1개 |
| PNP | onsemi 2N3906BU | 2 | TO-92. 과전압 차단 구동 |
| NPN | onsemi 2N3904BU | 2 | TO-92. K1 구동 |
| 쇼트키 | Taiwan Semiconductor Corporation 1N5819 | 4 | 1A, 40V, 스루홀 |
| 소신호 다이오드 | onsemi 1N4148 | 4 | 스루홀 |
| 벌크 커패시터 | Panasonic EEU-FR1E102 | 2 | 1000µF, 25V 이상 |
| 레일·Vbias·VIN 필터 커패시터 | Panasonic EEU-FC1V101 | 8 | 100µF, 35V 이상. 12V 어댑터에서 레일 약 23V. 한 대에 4개. EEU-FR1V101은 백오더(입고 예정 2026-10-14)라 FC 계열로 대체 |
| 펌프·커플링 커패시터 | Panasonic EEU-FR1H100 | 10 | 10µF, 50V. 한 대에 5개 |
| 1단계 연결 잭 | Switchcraft 12A | 2 | 6.35mm TS |
| 저항 2.2M | Yageo MFR-25FBF52-2M2 | 3 | 1/4W 금속피막 1%. 저항 행은 회로 설계 9절 사용 수의 2배 이상(한 번만 쓰는 값은 3개) |
| 저항 1.8M | Yageo MFR-25FBF52-1M8 | 3 | |
| 저항 100k | Yageo MFR-25FBF52-100K | 10 | 한 대에 3개. 10개 단가가 더 싸다 |
| 저항 47k | Yageo MFR-25FBF52-47K | 10 | 한 대에 2개. 10개 값이 4개 값과 거의 같다 |
| 저항 10k | Yageo MFR-25FBF52-10K | 20 | 한 대에 8개 |
| 저항 4.7k | Yageo MFR-25FBF52-4K7 | 3 | |
| 저항 3.3k | Yageo MFR-25FBF52-3K3 | 3 | |
| 저항 2.2k | Yageo MFR-25FBF52-2K2 | 3 | |
| 저항 1k | Yageo MFR-25FBF52-1K | 3 | |
| 저항 820Ω | Yageo MFR-25FBF52-820R | 3 | |
| 저항 470Ω | Yageo MFR-25FBF52-470R | 21 | 한 대에 9개 |
| 저항 100Ω | Yageo MFR-25FBF52-100R | 3 | |
| 저항 10Ω | Yageo MFR-25FBF52-10R | 3 | |
| 저항 3.3Ω | Yageo CFR-25JB-52-3R3 | 3 | 탄소피막 5%. MFR-25 계열에 3.3Ω이 없다. R32는 VIN 필터라 허용오차가 결과를 바꾸지 않는다 |
| 필름 100nF | TDK B32529C0104J000 | 2 | 63V PET 5%. C1 |
| 필름 1µF | TDK B32529C0105J000 | 2 | 63V PET 5%. C3. WIMA MKS2 1µF는 단종 표시거나 마켓플레이스 품목이다 |
| 세라믹 100nF | Vishay K104K15X7RF53L2 | 20 | 50V X7R. 디커플링과 노브 필터. 한 대에 9개 |
| 세라믹 4.7nF | TDK FG28C0G1H472JNT06 | 2 | 50V C0G. C23은 코덱 입력 신호 경로라 전압에 따라 용량이 변하는 X7R을 쓰지 않는다 |
| DIP-8 소켓 | TE 1-2199298-2 | 4 | 오픈 프레임. 한 대에 U2와 변환 기판용 2개. 머신드 소켓은 변환 기판의 각형 헤더 핀을 받으면 접점이 벌어져 피한다 |

- [x] **Step 3: 합계 확인**

단가 × 수량 합계가 60,000원 이상인지 확인한다(DigiKey 코리아 무료 배송 기준, 문서 기준). 모자라면 여분 수량을 늘리지 말고 사용자에게 보고한다.

- [ ] **Step 4: 계획서 부품표 갱신**

`devices/bass-effector.md` 부품 절에서 DigiKey로 사는 행의 "확인 필요"를 확정 품번과 수량으로 바꾼다. 가격과 재고 수치는 문서에 적지 않는다.

- [ ] **Step 5: 커밋**

```bash
git add devices/bass-effector.md
git commit -m "[Docs] 이펙터 DigiKey 주문 품번 확정"
```

주문과 결제는 사용자가 한다.

---

### Task 2: 네이티브 테스트 골격과 DcBlocker

**Files:**
- Create: `effector/CMakeLists.txt`
- Create: `effector/.gitignore`
- Create: `effector/test/check.h`, `effector/test/test_main.cpp`, `effector/test/signal_helpers.h`
- Create: `effector/test/test_dc_blocker.cpp`
- Create: `effector/lib/effector_core/src/dc_blocker.h`, `effector/lib/effector_core/src/dc_blocker.cpp`

**Interfaces:**
- Produces: `TEST_CASE`, `CHECK`, `CHECK_NEAR` 매크로, `makeSine(frequencyHz, amplitude, sampleCount)`, `rmsDecibels(samples, firstIndex)`, `secondsToSamples(seconds)`, `kTestSampleRateHz`
- Produces: `DcBlocker(float sampleRateHz, float cutoffHz)`, `float processSample(float)`, `void process(const float*, float*, std::size_t)`, `void reset()`

- [ ] **Step 1: 빌드 설정과 하네스 작성**

`effector/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(effector_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS lib/effector_core/src/*.cpp)
add_library(effector_core STATIC ${CORE_SOURCES})
target_include_directories(effector_core PUBLIC lib/effector_core/src)
target_compile_options(effector_core PRIVATE -Wall -Wextra -Werror)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test/*.cpp)
add_executable(native_tests ${TEST_SOURCES})
target_include_directories(native_tests PRIVATE test)
target_link_libraries(native_tests PRIVATE effector_core)
target_compile_options(native_tests PRIVATE -Wall -Wextra -Werror)
```

`effector/.gitignore`:

```gitignore
build/
.pio/
```

`effector/test/check.h`:

```cpp
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace check {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

inline void fail(const char* file, int line, const std::string& message) {
    std::printf("  FAIL %s:%d %s\n", file, line, message.c_str());
    ++failureCount();
}

inline int runAll() {
    int failedTests = 0;
    for (const TestCase& test : registry()) {
        const int failuresBefore = failureCount();
        test.body();
        const bool passed = failureCount() == failuresBefore;
        std::printf("%s %s\n", passed ? "PASS" : "FAIL", test.name);
        if (!passed) {
            ++failedTests;
        }
    }
    std::printf("%zu tests, %d failed\n", registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}

}  // namespace check

#define CHECK_CONCAT_INNER(first, second) first##second
#define CHECK_CONCAT(first, second) CHECK_CONCAT_INNER(first, second)

#define TEST_CASE(name)                                                    \
    static void CHECK_CONCAT(testBody, __LINE__)();                        \
    static check::Registrar CHECK_CONCAT(testRegistrar, __LINE__)(         \
        name, CHECK_CONCAT(testBody, __LINE__));                           \
    static void CHECK_CONCAT(testBody, __LINE__)()

#define CHECK(condition)                                                   \
    do {                                                                   \
        if (!(condition)) {                                                \
            check::fail(__FILE__, __LINE__, #condition);                   \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                            \
    do {                                                                   \
        const double actualValue = (actual);                               \
        const double expectedValue = (expected);                           \
        const double toleranceValue = (tolerance);                         \
        if (!(std::fabs(actualValue - expectedValue) <= toleranceValue)) { \
            char message[200];                                             \
            std::snprintf(message, sizeof message,                         \
                          "%s = %.6f, expected %.6f +- %.6f", #actual,     \
                          actualValue, expectedValue, toleranceValue);     \
            check::fail(__FILE__, __LINE__, message);                      \
        }                                                                  \
    } while (0)
```

`effector/test/test_main.cpp`:

```cpp
#include "check.h"

int main() {
    return check::runAll();
}
```

`effector/test/signal_helpers.h`:

```cpp
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

constexpr float kTestSampleRateHz = 48000.0f;

inline std::vector<float> makeSine(float frequencyHz, float amplitude,
                                   std::size_t sampleCount) {
    std::vector<float> samples(sampleCount);
    const double phaseStep = 2.0 * M_PI * frequencyHz / kTestSampleRateHz;
    for (std::size_t index = 0; index < sampleCount; ++index) {
        samples[index] = amplitude * static_cast<float>(std::sin(phaseStep * index));
    }
    return samples;
}

inline double rmsDecibels(const std::vector<float>& samples, std::size_t firstIndex) {
    double sumOfSquares = 0.0;
    for (std::size_t index = firstIndex; index < samples.size(); ++index) {
        sumOfSquares += static_cast<double>(samples[index]) * samples[index];
    }
    const double meanSquare = sumOfSquares / static_cast<double>(samples.size() - firstIndex);
    return 10.0 * std::log10(meanSquare);
}

inline std::size_t secondsToSamples(double seconds) {
    return static_cast<std::size_t>(seconds * kTestSampleRateHz);
}
```

- [ ] **Step 2: 실패하는 테스트 작성**

`effector/test/test_dc_blocker.cpp`:

```cpp
#include "check.h"
#include "dc_blocker.h"
#include "signal_helpers.h"

TEST_CASE("DcBlocker removes a constant offset") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    float output = 0.0f;
    for (std::size_t index = 0; index < secondsToSamples(1.0); ++index) {
        output = blocker.processSample(0.5f);
    }
    CHECK_NEAR(output, 0.0, 1e-4);
}

TEST_CASE("DcBlocker keeps 1 kHz within 0.05 dB") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    std::vector<float> input = makeSine(1000.0f, 0.5f, secondsToSamples(1.0));
    std::vector<float> output(input.size());
    blocker.process(input.data(), output.data(), input.size());
    const std::size_t settled = secondsToSamples(0.5);
    CHECK_NEAR(rmsDecibels(output, settled) - rmsDecibels(input, settled), 0.0, 0.05);
}

TEST_CASE("DcBlocker attenuates 20 Hz by less than 1 dB") {
    DcBlocker blocker(kTestSampleRateHz, 5.0f);
    std::vector<float> input = makeSine(20.0f, 0.5f, secondsToSamples(2.0));
    std::vector<float> output(input.size());
    blocker.process(input.data(), output.data(), input.size());
    const std::size_t settled = secondsToSamples(1.0);
    const double changeDecibels = rmsDecibels(output, settled) - rmsDecibels(input, settled);
    CHECK(changeDecibels > -1.0);
    CHECK(changeDecibels < 0.0);
}
```

- [ ] **Step 3: 실패 확인**

Run: `cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release && cmake --build effector/build`
Expected: `dc_blocker.h: No such file or directory` 컴파일 오류, 또는 코어 소스가 없어 `add_library` 오류

- [ ] **Step 4: 구현**

`effector/lib/effector_core/src/dc_blocker.h`:

```cpp
#pragma once

#include <cstddef>

// 1차 DC 제거 필터. y[n] = x[n] - x[n-1] + pole * y[n-1]
class DcBlocker {
public:
    DcBlocker(float sampleRateHz, float cutoffHz);

    void reset();
    float processSample(float input);
    void process(const float* input, float* output, std::size_t count);

private:
    float pole_;
    float previousInput_ = 0.0f;
    float previousOutput_ = 0.0f;
};
```

`effector/lib/effector_core/src/dc_blocker.cpp`:

```cpp
#include "dc_blocker.h"

#include <cmath>

DcBlocker::DcBlocker(float sampleRateHz, float cutoffHz)
    : pole_(static_cast<float>(std::exp(-2.0 * M_PI * cutoffHz / sampleRateHz))) {}

void DcBlocker::reset() {
    previousInput_ = 0.0f;
    previousOutput_ = 0.0f;
}

float DcBlocker::processSample(float input) {
    const float output = input - previousInput_ + pole_ * previousOutput_;
    previousInput_ = input;
    previousOutput_ = output;
    return output;
}

void DcBlocker::process(const float* input, float* output, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = processSample(input[index]);
    }
}
```

차단 주파수 5Hz는 설계서 6.2절(1차 필터 10Hz 이하)을 만족한다. 20Hz에서 약 -0.26dB다.

- [ ] **Step 5: 통과 확인**

Run: `cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release && cmake --build effector/build && ./effector/build/native_tests`
Expected: `PASS DcBlocker ...` 세 줄과 `3 tests, 0 failed`

- [ ] **Step 6: 커밋**

```bash
git add effector/CMakeLists.txt effector/.gitignore effector/test effector/lib/effector_core/src/dc_blocker.h effector/lib/effector_core/src/dc_blocker.cpp
git commit -m "[Feat] 이펙터 네이티브 테스트 골격과 DC 제거 필터"
```

---

### Task 3: Compressor와 T9 대조군

**Files:**
- Create: `effector/test/reference/openaudio_compressor.h`
- Create: `effector/test/test_compressor.cpp`
- Create: `effector/lib/effector_core/src/compressor.h`, `effector/lib/effector_core/src/compressor.cpp`
- Create: `effector/tools/static_gain.cpp`
- Modify: `effector/CMakeLists.txt` (끝에 도구 타깃 추가)

**Interfaces:**
- Consumes: Task 2 하네스
- Produces: `struct CompressorSettings { float thresholdDbfs; float ratio; float attackSeconds; float releaseSeconds; float sidechainHighPassHz; }`
- Produces: `Compressor(float sampleRateHz)`, `void configure(const CompressorSettings&)`, `void reset()`, `void process(const float*, float*, std::size_t)`, `float currentGainDb() const`
- Produces: 실행 파일 `static_gain <thresholdDbfs> <ratio> <attackSeconds> <releaseSeconds> <toneDbfs>` → 평균 게인 dB 한 줄

알아둘 것:
- 레벨 추정(제곱, 1극 저역통과, dB)과 게인 평활(목표가 현재보다 작으면 어택 계수, 아니면 릴리즈 계수)은 OpenAudio `AudioEffectCompressor_F32`와 같은 식이다. 대조군 파일은 그 식을 샘플 단위로 옮겼고, 원본의 `log2f_approx` 다항 근사를 그대로 쓴다(근사 오차는 레벨 기준 최대 약 0.004dB)
- 대조군은 원본처럼 레벨 전력을 1.0(0dBFS)에서 시작해 첫 1초 동안 초기 과도가 남는다. 비교는 1초 뒤부터 한다
- 어택이 빠르면(5ms) 게인이 1kHz 사인의 레벨 리플 봉우리를 따라가 평균 게인이 정적 곡선보다 약 0.1dB 더 눌린다. 정적 곡선 테스트는 어택 50ms로 둔다

- [ ] **Step 1: 대조군 작성**

`effector/test/reference/openaudio_compressor.h`:

```cpp
#pragma once

// 대조군: OpenAudio_ArduinoLibrary AudioEffectCompressor_F32 (Chip Audette, MIT)의
// 레벨 추정·목표 게인·평활 식을 샘플 단위로 옮긴 것. HPF와 프리게인은 뺐다.

#include <algorithm>
#include <cmath>
#include <cstddef>

class OpenAudioCompressorReference {
public:
    OpenAudioCompressorReference(float sampleRateHz, float thresholdDbfs, float ratio,
                                 float attackSeconds, float releaseSeconds)
        : thresholdDbfs_(thresholdDbfs), ratio_(std::max(0.001f, ratio)) {
        attackConst_ = std::exp(-1.0f / (attackSeconds * sampleRateHz));
        releaseConst_ = std::exp(-1.0f / (releaseSeconds * sampleRateHz));
        const float levelSeconds =
            std::max(0.002f, std::min(attackSeconds, releaseSeconds) / 5.0f);
        levelConst_ = std::exp(-1.0f / (levelSeconds * sampleRateHz));
    }

    float processSample(float input) {
        float power = levelConst_ * previousLevelPower_ + (1.0f - levelConst_) * input * input;
        previousLevelPower_ = power;
        const float levelDb = 10.0f * log10Approx(power);
        float targetGainDb = (levelDb - thresholdDbfs_) / ratio_ - (levelDb - thresholdDbfs_);
        if (targetGainDb > 0.0f) {
            targetGainDb = 0.0f;
        }
        if (targetGainDb < previousGainDb_) {
            previousGainDb_ = attackConst_ * previousGainDb_ + (1.0f - attackConst_) * targetGainDb;
        } else {
            previousGainDb_ =
                releaseConst_ * previousGainDb_ + (1.0f - releaseConst_) * targetGainDb;
        }
        if (previousLevelPower_ < 1.0e-13f) {
            previousLevelPower_ = 1.0e-13f;
        }
        return input * std::exp(2.302585092994f * previousGainDb_ / 20.0f);
    }

    float currentGainDb() const { return previousGainDb_; }

private:
    static float log2Approx(float value) {
        int exponent = 0;
        const float fraction = std::frexp(std::fabs(value), &exponent);
        float result = 1.23149591368684f;
        result *= fraction;
        result += -4.11852516267426f;
        result *= fraction;
        result += 6.02197014179219f;
        result *= fraction;
        result += -3.13396450166353f;
        result += static_cast<float>(exponent);
        return result;
    }

    static float log10Approx(float value) { return log2Approx(value) * 0.3010299956639812f; }

    float thresholdDbfs_;
    float ratio_;
    float attackConst_;
    float releaseConst_;
    float levelConst_;
    float previousLevelPower_ = 1.0f;
    float previousGainDb_ = 0.0f;
};
```

- [ ] **Step 2: 실패하는 테스트 작성**

`effector/test/test_compressor.cpp`:

```cpp
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.h"
#include "compressor.h"
#include "reference/openaudio_compressor.h"
#include "signal_helpers.h"

namespace {

double averageGainDb(Compressor& compressor, const std::vector<float>& input,
                     std::size_t firstIndex) {
    std::vector<float> output(1);
    double sum = 0.0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        compressor.process(&input[index], output.data(), 1);
        if (index >= firstIndex) {
            sum += compressor.currentGainDb();
        }
    }
    return sum / static_cast<double>(input.size() - firstIndex);
}

double expectedStaticGainDb(float sineAmplitude, float thresholdDbfs, float ratio) {
    const double levelDb = 20.0 * std::log10(sineAmplitude) - 10.0 * std::log10(2.0);
    return std::min(0.0, (levelDb - thresholdDbfs) * (1.0 / ratio - 1.0));
}

std::vector<float> makeLevelSteps() {
    std::vector<float> samples;
    const float amplitudes[] = {0.01f, 0.5f, 0.05f, 0.9f, 0.02f};
    for (float amplitude : amplitudes) {
        std::vector<float> segment = makeSine(1000.0f, amplitude, secondsToSamples(0.5));
        samples.insert(samples.end(), segment.begin(), segment.end());
    }
    return samples;
}

}  // namespace

TEST_CASE("Compressor leaves signal below threshold untouched") {
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({-20.0f, 4.0f, 0.005f, 0.2f, 0.0f});
    std::vector<float> input = makeSine(1000.0f, 0.01f, secondsToSamples(1.0));
    CHECK_NEAR(averageGainDb(compressor, input, secondsToSamples(0.5)), 0.0, 0.01);
}

TEST_CASE("Compressor static gain matches the ratio above threshold") {
    Compressor compressor(kTestSampleRateHz);
    // 어택이 빠르면 게인이 레벨 리플의 봉우리를 따라가 평균이 정적 곡선보다 눌린다.
    // 정적 곡선만 보려고 어택을 50ms로 둔다.
    compressor.configure({-20.0f, 4.0f, 0.05f, 0.2f, 0.0f});
    std::vector<float> input = makeSine(1000.0f, 0.5f, secondsToSamples(3.0));
    CHECK_NEAR(averageGainDb(compressor, input, secondsToSamples(2.0)),
               expectedStaticGainDb(0.5f, -20.0f, 4.0f), 0.1);
}

TEST_CASE("Compressor gain tracks the OpenAudio reference within 0.1 dB (T9)") {
    const float thresholdDbfs = -24.0f;
    const float ratio = 6.0f;
    const float attackSeconds = 0.003f;
    const float releaseSeconds = 0.25f;
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({thresholdDbfs, ratio, attackSeconds, releaseSeconds, 0.0f});
    OpenAudioCompressorReference reference(kTestSampleRateHz, thresholdDbfs, ratio,
                                           attackSeconds, releaseSeconds);
    std::vector<float> input = makeLevelSteps();
    float output = 0.0f;
    double largestDifferenceDb = 0.0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        compressor.process(&input[index], &output, 1);
        reference.processSample(input[index]);
        // 참조는 레벨 전력을 1.0(0dBFS)에서 시작해 첫 1초 동안 초기 과도가 남는다
        if (index >= secondsToSamples(1.0)) {
            largestDifferenceDb =
                std::max(largestDifferenceDb,
                                  static_cast<double>(
                             std::fabs(compressor.currentGainDb() - reference.currentGainDb())));
        }
    }
    CHECK(largestDifferenceDb <= 0.1);
}

TEST_CASE("Compressor attack time matches the reference within 10 percent (T9)") {
    const float attackSeconds = 0.01f;
    Compressor compressor(kTestSampleRateHz);
    compressor.configure({-30.0f, 8.0f, attackSeconds, 0.3f, 0.0f});
    OpenAudioCompressorReference reference(kTestSampleRateHz, -30.0f, 8.0f, attackSeconds, 0.3f);
    std::vector<float> quiet = makeSine(1000.0f, 0.005f, secondsToSamples(1.0));
    std::vector<float> loud = makeSine(1000.0f, 0.8f, secondsToSamples(0.5));
    float output = 0.0f;
    for (float sample : quiet) {
        compressor.process(&sample, &output, 1);
        reference.processSample(sample);
    }
    const double finalGainDb = expectedStaticGainDb(0.8f, -30.0f, 8.0f);
    const double crossingDb = 0.632 * finalGainDb;
    long compressorCrossing = -1;
    long referenceCrossing = -1;
    for (std::size_t index = 0; index < loud.size(); ++index) {
        compressor.process(&loud[index], &output, 1);
        reference.processSample(loud[index]);
        if (compressorCrossing < 0 && compressor.currentGainDb() <= crossingDb) {
            compressorCrossing = static_cast<long>(index);
        }
        if (referenceCrossing < 0 && reference.currentGainDb() <= crossingDb) {
            referenceCrossing = static_cast<long>(index);
        }
    }
    CHECK(compressorCrossing > 0);
    CHECK(referenceCrossing > 0);
    CHECK_NEAR(static_cast<double>(compressorCrossing), static_cast<double>(referenceCrossing),
               0.1 * referenceCrossing);
}

TEST_CASE("Sidechain high-pass reduces compression of low notes only") {
    CompressorSettings withoutFilter{-20.0f, 4.0f, 0.005f, 0.2f, 0.0f};
    CompressorSettings withFilter = withoutFilter;
    withFilter.sidechainHighPassHz = 150.0f;

    Compressor lowWithout(kTestSampleRateHz);
    lowWithout.configure(withoutFilter);
    Compressor lowWith(kTestSampleRateHz);
    lowWith.configure(withFilter);
    std::vector<float> lowNote = makeSine(40.0f, 0.5f, secondsToSamples(2.0));
    const double lowGainWithout = averageGainDb(lowWithout, lowNote, secondsToSamples(1.5));
    const double lowGainWith = averageGainDb(lowWith, lowNote, secondsToSamples(1.5));
    CHECK(lowGainWithout < -6.0);
    CHECK(lowGainWith > -1.0);

    Compressor highWithout(kTestSampleRateHz);
    highWithout.configure(withoutFilter);
    Compressor highWith(kTestSampleRateHz);
    highWith.configure(withFilter);
    std::vector<float> highNote = makeSine(1000.0f, 0.5f, secondsToSamples(2.0));
    CHECK_NEAR(averageGainDb(highWith, highNote, secondsToSamples(1.5)),
               averageGainDb(highWithout, highNote, secondsToSamples(1.5)), 0.1);
}
```

- [ ] **Step 3: 실패 확인**

Run: `cmake --build effector/build`
Expected: `compressor.h: No such file or directory` 컴파일 오류

- [ ] **Step 4: 구현**

`effector/lib/effector_core/src/compressor.h`:

```cpp
#pragma once

#include <cstddef>

struct CompressorSettings {
    float thresholdDbfs = -20.0f;
    float ratio = 4.0f;
    float attackSeconds = 0.005f;
    float releaseSeconds = 0.200f;
    float sidechainHighPassHz = 0.0f;  // 0이면 끔
};

// RMS 검출 피드포워드 컴프레서. 검출기 앞에만 사이드체인 HPF를 둔다.
// 레벨 추정과 게인 평활 식은 OpenAudio AudioEffectCompressor_F32와 같다.
class Compressor {
public:
    explicit Compressor(float sampleRateHz);

    void configure(const CompressorSettings& settings);
    void reset();
    void process(const float* input, float* output, std::size_t count);
    float currentGainDb() const { return gainDb_; }

private:
    struct HighPass {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        float process(float input);
    };

    float sampleRateHz_;
    CompressorSettings settings_;
    float attackCoefficient_ = 0.0f;
    float releaseCoefficient_ = 0.0f;
    float levelCoefficient_ = 0.0f;
    bool sidechainFilterEnabled_ = false;
    HighPass sidechainFilter_;
    float levelPower_ = 1.0e-13f;
    float gainDb_ = 0.0f;
};
```

`effector/lib/effector_core/src/compressor.cpp`:

```cpp
#include "compressor.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMinimumLevelPower = 1.0e-13f;
constexpr float kMinimumLevelTimeSeconds = 0.002f;
}  // namespace

float Compressor::HighPass::process(float input) {
    const float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;
    return output;
}

Compressor::Compressor(float sampleRateHz) : sampleRateHz_(sampleRateHz) {
    configure(CompressorSettings{});
}

void Compressor::configure(const CompressorSettings& settings) {
    settings_ = settings;
    attackCoefficient_ = std::exp(-1.0f / (settings.attackSeconds * sampleRateHz_));
    releaseCoefficient_ = std::exp(-1.0f / (settings.releaseSeconds * sampleRateHz_));
    const float levelSeconds = std::max(
        kMinimumLevelTimeSeconds, std::min(settings.attackSeconds, settings.releaseSeconds) / 5.0f);
    levelCoefficient_ = std::exp(-1.0f / (levelSeconds * sampleRateHz_));

    sidechainFilterEnabled_ = settings.sidechainHighPassHz > 0.0f;
    if (sidechainFilterEnabled_) {
        // RBJ 2차 하이패스, Q = 1/sqrt(2)
        const double omega = 2.0 * M_PI * settings.sidechainHighPassHz / sampleRateHz_;
        const double alpha = std::sin(omega) / (2.0 * M_SQRT1_2);
        const double cosine = std::cos(omega);
        const double a0 = 1.0 + alpha;
        sidechainFilter_.b0 = static_cast<float>((1.0 + cosine) / 2.0 / a0);
        sidechainFilter_.b1 = static_cast<float>(-(1.0 + cosine) / a0);
        sidechainFilter_.b2 = sidechainFilter_.b0;
        sidechainFilter_.a1 = static_cast<float>(-2.0 * cosine / a0);
        sidechainFilter_.a2 = static_cast<float>((1.0 - alpha) / a0);
    }
}

void Compressor::reset() {
    levelPower_ = kMinimumLevelPower;
    gainDb_ = 0.0f;
    sidechainFilter_.x1 = sidechainFilter_.x2 = sidechainFilter_.y1 = sidechainFilter_.y2 = 0.0f;
}

void Compressor::process(const float* input, float* output, std::size_t count) {
    const float slope = 1.0f / settings_.ratio - 1.0f;
    for (std::size_t index = 0; index < count; ++index) {
        const float sidechain =
            sidechainFilterEnabled_ ? sidechainFilter_.process(input[index]) : input[index];
        levelPower_ = levelCoefficient_ * levelPower_ +
                      (1.0f - levelCoefficient_) * sidechain * sidechain;
        const float levelDb = 10.0f * std::log10(std::max(levelPower_, kMinimumLevelPower));
        const float targetGainDb = std::min(0.0f, (levelDb - settings_.thresholdDbfs) * slope);
        const float coefficient =
            targetGainDb < gainDb_ ? attackCoefficient_ : releaseCoefficient_;
        gainDb_ = coefficient * gainDb_ + (1.0f - coefficient) * targetGainDb;
        output[index] = input[index] * std::pow(10.0f, gainDb_ / 20.0f);
    }
}
```

- [ ] **Step 5: 통과 확인**

Run: `cmake --build effector/build && ./effector/build/native_tests`
Expected: `PASS Compressor ...` 네 줄, `PASS Sidechain high-pass ...` 한 줄, `8 tests, 0 failed`

- [ ] **Step 6: 실기기 T9용 도구 추가**

`effector/tools/static_gain.cpp`:

```cpp
// 실기기 T9 대조값: 같은 설정의 네이티브 컴프레서에 1kHz 사인을 3초 넣고 마지막 1초 평균 게인을 출력한다.
// 사용: static_gain <thresholdDbfs> <ratio> <attackSeconds> <releaseSeconds> <toneDbfs>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "compressor.h"

int main(int argumentCount, char** arguments) {
    if (argumentCount != 6) {
        std::fprintf(stderr, "usage: static_gain threshold ratio attack release toneDbfs\n");
        return 2;
    }
    const float sampleRateHz = 48000.0f;
    CompressorSettings settings;
    settings.thresholdDbfs = std::strtof(arguments[1], nullptr);
    settings.ratio = std::strtof(arguments[2], nullptr);
    settings.attackSeconds = std::strtof(arguments[3], nullptr);
    settings.releaseSeconds = std::strtof(arguments[4], nullptr);
    const float amplitude = std::pow(10.0f, std::strtof(arguments[5], nullptr) / 20.0f);

    Compressor compressor(sampleRateHz);
    compressor.configure(settings);
    const std::size_t totalSamples = static_cast<std::size_t>(3.0f * sampleRateHz);
    const std::size_t averageFrom = static_cast<std::size_t>(2.0f * sampleRateHz);
    const double phaseStep = 2.0 * M_PI * 1000.0 / sampleRateHz;
    double sum = 0.0;
    float output = 0.0f;
    for (std::size_t index = 0; index < totalSamples; ++index) {
        const float input = amplitude * static_cast<float>(std::sin(phaseStep * index));
        compressor.process(&input, &output, 1);
        if (index >= averageFrom) {
            sum += compressor.currentGainDb();
        }
    }
    std::printf("%.3f\n", sum / static_cast<double>(totalSamples - averageFrom));
    return 0;
}
```

`effector/CMakeLists.txt` 끝에 추가:

```cmake

add_executable(static_gain tools/static_gain.cpp)
target_link_libraries(static_gain PRIVATE effector_core)
target_compile_options(static_gain PRIVATE -Wall -Wextra -Werror)
```

Run: `cmake -S effector -B effector/build && cmake --build effector/build && ./effector/build/static_gain -20 4 0.05 0.2 -6.0206`
Expected: `-8.238`

- [ ] **Step 7: 커밋**

```bash
git add effector/CMakeLists.txt effector/test/reference effector/test/test_compressor.cpp effector/lib/effector_core/src/compressor.h effector/lib/effector_core/src/compressor.cpp effector/tools/static_gain.cpp
git commit -m "[Feat] 이펙터 컴프레서와 OpenAudio 대조 테스트"
```

---

### Task 4: PgaController (삭제)

Seed3의 TAC5242는 핀 제어 코덱이라 소프트웨어 입력 게인이 없다(설계서 4.3절). Teensy 기준으로 만들었던
`pga_controller`와 그 테스트는 2026-09-15에 지웠고 Input 노브는 디지털 프리게인만 쓴다. 번호는 뒤 작업의 참조를
유지하려고 남긴다.

---

### Task 5: Meter

**Files:**
- Create: `effector/test/test_meter.cpp`
- Create: `effector/lib/effector_core/src/meter.h`, `effector/lib/effector_core/src/meter.cpp`

**Interfaces:**
- Consumes: Task 2 하네스
- Produces: `Meter::Config { std::array<float, 8> thresholdsDb; float clipLevelDbfs; int clipHoldBlocks; int blinkHalfPeriodBlocks; }`
- Produces: `Meter(const Config&)`, `std::uint8_t processBlock(float gainReductionDb, float blockPeakDbfs)` (비트 i가 LED i)

클립이 이어지는 동안에도 점멸 위상을 처음으로 되돌리지 않는다. 되돌리면 LED가 켜진 채로 멈춘다.

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/test/test_meter.cpp`:

```cpp
#include "check.h"
#include "meter.h"

namespace {

Meter::Config testConfig() {
    return {{1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 10.0f, 14.0f, 20.0f}, -0.1f, 8, 2};
}

}  // namespace

TEST_CASE("Meter lights LEDs up to the gain reduction") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(0.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(1.0f, -30.0f) == 0x01);
    CHECK(meter.processBlock(6.0f, -30.0f) == 0x0F);
    CHECK(meter.processBlock(25.0f, -30.0f) == 0xFF);
}

TEST_CASE("Meter blinks all LEDs for the clip hold time (T15)") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(3.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0xFF);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x00);
    CHECK(meter.processBlock(3.0f, -30.0f) == 0x07);
}

TEST_CASE("Meter keeps blinking while the clip continues") {
    Meter meter(testConfig());
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0x00);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0x00);
    CHECK(meter.processBlock(0.0f, 0.0f) == 0xFF);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build effector/build`
Expected: `meter.h: No such file or directory` 컴파일 오류

- [ ] **Step 3: 구현**

`effector/lib/effector_core/src/meter.h`:

```cpp
#pragma once

#include <array>
#include <cstdint>

// 감쇠량 LED 8칸과 입력 클립 점멸. 반환값의 비트 i가 LED i다.
class Meter {
public:
    struct Config {
        std::array<float, 8> thresholdsDb;
        float clipLevelDbfs;
        int clipHoldBlocks;
        int blinkHalfPeriodBlocks;
    };

    explicit Meter(const Config& config);

    std::uint8_t processBlock(float gainReductionDb, float blockPeakDbfs);

private:
    Config config_;
    int clipBlocksRemaining_ = 0;
    int clipElapsedBlocks_ = 0;
};
```

`effector/lib/effector_core/src/meter.cpp`:

```cpp
#include "meter.h"

Meter::Meter(const Config& config) : config_(config) {}

std::uint8_t Meter::processBlock(float gainReductionDb, float blockPeakDbfs) {
    if (blockPeakDbfs >= config_.clipLevelDbfs) {
        if (clipBlocksRemaining_ == 0) {
            clipElapsedBlocks_ = 0;
        }
        clipBlocksRemaining_ = config_.clipHoldBlocks;
    }
    if (clipBlocksRemaining_ > 0) {
        const bool lit = (clipElapsedBlocks_ / config_.blinkHalfPeriodBlocks) % 2 == 0;
        ++clipElapsedBlocks_;
        --clipBlocksRemaining_;
        return lit ? 0xFF : 0x00;
    }
    std::uint8_t bits = 0;
    for (std::size_t led = 0; led < config_.thresholdsDb.size(); ++led) {
        if (gainReductionDb >= config_.thresholdsDb[led]) {
            bits |= static_cast<std::uint8_t>(1u << led);
        }
    }
    return bits;
}
```

- [ ] **Step 4: 통과 확인**

Run: `cmake --build effector/build && ./effector/build/native_tests`
Expected: `PASS Meter ...` 세 줄, `15 tests, 0 failed`

- [ ] **Step 5: 커밋**

```bash
git add effector/test/test_meter.cpp effector/lib/effector_core/src/meter.h effector/lib/effector_core/src/meter.cpp
git commit -m "[Feat] 이펙터 감쇠 미터와 클립 점멸"
```

---

### Task 6: parameter_mapping

**Files:**
- Create: `effector/test/test_parameter_mapping.cpp`
- Create: `effector/lib/effector_core/src/parameter_mapping.h`, `effector/lib/effector_core/src/parameter_mapping.cpp`

**Interfaces:**
- Consumes: Task 2 하네스
- Produces: 네임스페이스 `parameter_mapping`의 `clampKnob`, `ratioFromKnob`, `attackReleaseFromKnob` → `AttackRelease { float attackSeconds; float releaseSeconds; }`, `outputGainDbFromKnob`, `inputDriveDbFromKnob(float knob, float maximumDriveDb)`, `decibelsToGain`

- [ ] **Step 1: 실패하는 테스트 작성**

`effector/test/test_parameter_mapping.cpp`:

```cpp
#include "check.h"
#include "parameter_mapping.h"

using namespace parameter_mapping;

TEST_CASE("Output knob at noon is unity") {
    CHECK_NEAR(outputGainDbFromKnob(0.5f), 0.0, 1e-6);
    CHECK_NEAR(outputGainDbFromKnob(0.0f), kMinimumOutputDb, 1e-6);
    CHECK_NEAR(outputGainDbFromKnob(1.0f), kMaximumOutputDb, 1e-6);
    CHECK(outputGainDbFromKnob(0.4f) < outputGainDbFromKnob(0.6f));
}

TEST_CASE("Ratio knob spans 1:1 to 20:1") {
    CHECK_NEAR(ratioFromKnob(0.0f), 1.0, 1e-5);
    CHECK_NEAR(ratioFromKnob(1.0f), 20.0, 1e-4);
    CHECK(ratioFromKnob(0.3f) < ratioFromKnob(0.7f));
}

TEST_CASE("Attack and release move in opposite directions") {
    const AttackRelease fast = attackReleaseFromKnob(0.0f);
    const AttackRelease slow = attackReleaseFromKnob(1.0f);
    CHECK_NEAR(fast.attackSeconds, kFastestAttackSeconds, 1e-7);
    CHECK_NEAR(fast.releaseSeconds, kSlowestReleaseSeconds, 1e-6);
    CHECK_NEAR(slow.attackSeconds, kSlowestAttackSeconds, 1e-6);
    CHECK_NEAR(slow.releaseSeconds, kFastestReleaseSeconds, 1e-6);
}

TEST_CASE("Knob values outside 0 to 1 are clamped") {
    CHECK_NEAR(inputDriveDbFromKnob(-0.5f, 30.0f), 0.0, 1e-6);
    CHECK_NEAR(inputDriveDbFromKnob(1.5f, 30.0f), 30.0, 1e-5);
    CHECK_NEAR(decibelsToGain(-6.0206f), 0.5, 1e-4);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build effector/build`
Expected: `parameter_mapping.h: No such file or directory` 컴파일 오류

- [ ] **Step 3: 구현**

`effector/lib/effector_core/src/parameter_mapping.h`:

```cpp
#pragma once

// 노브 값(0~1)을 컴프레서 파라미터로 바꾼다. 범위 상수는 초기값이고 T16 청음으로 조정한다.
namespace parameter_mapping {

struct AttackRelease {
    float attackSeconds;
    float releaseSeconds;
};

constexpr float kMinimumRatio = 1.0f;
constexpr float kMaximumRatio = 20.0f;
constexpr float kFastestAttackSeconds = 0.0005f;
constexpr float kSlowestAttackSeconds = 0.020f;
constexpr float kSlowestReleaseSeconds = 1.0f;
constexpr float kFastestReleaseSeconds = 0.050f;
constexpr float kMinimumOutputDb = -30.0f;
constexpr float kMaximumOutputDb = 12.0f;

float clampKnob(float knob);
float ratioFromKnob(float knob);
AttackRelease attackReleaseFromKnob(float knob);
float outputGainDbFromKnob(float knob);
float inputDriveDbFromKnob(float knob, float maximumDriveDb);
float decibelsToGain(float decibels);

}  // namespace parameter_mapping
```

`effector/lib/effector_core/src/parameter_mapping.cpp`:

```cpp
#include "parameter_mapping.h"

#include <algorithm>
#include <cmath>

namespace parameter_mapping {

namespace {
float logInterpolate(float from, float to, float fraction) {
    return from * std::pow(to / from, fraction);
}
}  // namespace

float clampKnob(float knob) {
    return std::clamp(knob, 0.0f, 1.0f);
}

float ratioFromKnob(float knob) {
    return logInterpolate(kMinimumRatio, kMaximumRatio, clampKnob(knob));
}

AttackRelease attackReleaseFromKnob(float knob) {
    const float fraction = clampKnob(knob);
    return {logInterpolate(kFastestAttackSeconds, kSlowestAttackSeconds, fraction),
            logInterpolate(kSlowestReleaseSeconds, kFastestReleaseSeconds, fraction)};
}

float outputGainDbFromKnob(float knob) {
    const float fraction = clampKnob(knob);
    if (fraction <= 0.5f) {
        return kMinimumOutputDb * (1.0f - fraction / 0.5f);
    }
    return kMaximumOutputDb * ((fraction - 0.5f) / 0.5f);
}

float inputDriveDbFromKnob(float knob, float maximumDriveDb) {
    return clampKnob(knob) * maximumDriveDb;
}

float decibelsToGain(float decibels) {
    return std::pow(10.0f, decibels / 20.0f);
}

}  // namespace parameter_mapping
```

- [ ] **Step 4: 통과 확인**

Run: `cmake --build effector/build && ./effector/build/native_tests`
Expected: `19 tests, 0 failed`

- [ ] **Step 5: 커밋**

```bash
git add effector/test/test_parameter_mapping.cpp effector/lib/effector_core/src/parameter_mapping.h effector/lib/effector_core/src/parameter_mapping.cpp
git commit -m "[Feat] 이펙터 노브 파라미터 매핑"
```

---

### Task 7: Seed3 펌웨어

**Files:**
- Create: `effector/Makefile`, `effector/src/main.cpp`
- Create: `effector/lib/effector_core/src/effector_processor.h`, `effector/lib/effector_core/src/effector_processor.cpp`
- Create: `effector/lib/effector_core/src/serial_console.h`, `effector/lib/effector_core/src/serial_console.cpp`
- Create: `effector/test/test_effector_processor.cpp`, `effector/test/test_serial_console.cpp`
- Submodule: `effector/lib/libDaisy` (커밋 265aae3)
- Modify: `effector/README.md`, `README.md` (폴더 규칙 표)

**Interfaces:**
- Consumes: Task 2, 3, 5, 6의 코어
- Produces: `EffectorProcessor(Settings{sampleRateHz, blockSamples})`, `process(input, output, count)`, `setParameters`, `setTestTone`, `currentGainDb`, `lastBlockPeakDbfs`, `meterBits`
- Produces: `ConsoleHost`(write, applyParameters, setTestTone, resetToBootloader)와 `SerialConsole::feed(bytes, count)`
- Produces: 시리얼 명령 `input|ratio|attrel|blend|output <0..1>`, `threshold <dBFS>`, `sidechain <Hz>`, `trim <dB>`, `effect <0|1>`, `tone <Hz> <dBFS>`, `tone 0`, `status`, `dfu`
- Produces: `status` 출력 형식 `gainDb=<f> peakDbfs=<f> meter=0x<XX> drive=<f> ratio=<f>`

알아둘 것:
- 프로세서와 콘솔은 코어로 옮겨 네이티브 테스트가 있다. Seed3 층은 `main.cpp` 하나다
- 오디오 콜백은 입력 채널 0을 처리해 출력 두 채널에 같이 낸다. 파라미터 적용과 테스트 톤은 `__disable_irq`로 콜백과 겹치지 않게 복사한다
- USB CDC 수신은 인터럽트에서 링 버퍼(256B)에 넣고 메인 루프가 꺼내 `feed`한다. 출력은 `TransmitInternal`
- `dfu`는 `System::ResetToBootloader()`로 STM DFU에 들어간다(설계서 3.6절)
- Ubuntu에서 `make`로 빌드를 확인했다(FLASH 106,736B, 128KB의 81%, 우리 코드 경고 0). 실기기 동작은 Task 8에서 처음 확인한다
- 내부 플래시가 차면 `APP_TYPE = BOOT_QSPI`로 바꾼다(`effector/README.md`)

- [x] **Step 1: 코어 이전과 테스트**

Teensy 층의 프로세서·콘솔을 코어로 옮기고 PGA를 뺐다. 콘솔은 `ConsoleHost` 인터페이스로 출력·적용·톤·DFU를 플랫폼에 넘긴다. 테스트: `test_effector_processor.cpp`(이펙트 꺼짐 유니티, 트림, 테스트 톤과 피크, 압축 정적 게인, 블렌드 0), `test_serial_console.cpp`(노브 명령 매핑, 미지 명령, CR LF와 빈 줄, 분할 입력, 사이드체인 클램프, tone·dfu 전달).

- [x] **Step 2: 네이티브 테스트**

Run: `cmake -S effector -B effector/build -DCMAKE_BUILD_TYPE=Release && cmake --build effector/build && ./effector/build/native_tests`
Expected: `26 tests, 0 failed`

- [x] **Step 3: Seed3 층과 빌드**

`src/main.cpp`와 `Makefile`을 쓰고 libDaisy를 서브모듈로 넣는다.

Run: `git submodule update --init --recursive --depth 1 && make -C effector/lib/libDaisy -j && cd effector && make`
Expected: `build/effector.bin`이 생기고 링커의 메모리 사용 표에서 FLASH가 128KB 아래

- [ ] **Step 4: 커밋**

```bash
git add effector devices README.md .gitmodules
git commit -m "[Feat] 이펙터 보드를 Daisy Seed3로 전환: 설계·회로·부품표·libDaisy 펌웨어"
```

---

### Task 8: 1단계 브레드보드 결선과 첫 소리

**Files:**
- 없음 (하드웨어 작업)

**Interfaces:**
- Consumes: Task 1 부품, Task 7 펌웨어

- [ ] **Step 1: Seed3 헤더**

핀 헤더가 동봉되지 않았으면 40핀을 20핀 두 줄로 잘라 Seed3 아래쪽에 납땜한다. 브레드보드 가운데 홈에 걸쳐 꽂는다. 핀 번호는 데이터시트 표 2(회로 설계 8절)다.

- [ ] **Step 2: 오디오 잭 배선**

Switchcraft 12A 두 개를 잭 A(입력), 잭 B(출력)로 쓴다.

| 연결 | 배선 |
|---|---|
| 잭 A 팁 | 1kΩ 직렬 → Seed3 핀 16(Audio In 1). 모듈 안에서 AC 커플링된다 |
| 잭 B 팁 | Seed3 핀 18(Audio Out 1) → 10µF 50V(+ 쪽이 모듈) → 잭 B 팁. 잭 B 팁에서 10kΩ을 GND로 |
| 슬리브 둘 | 핀 20(AGND)과 핀 40(DGND)을 이은 GND |

모듈 출력의 DC 전위가 공개되지 않아 커플링 C를 둔다(회로 설계 5절 C3와 같은 역할). 1단계에는 패드가 없으므로 잭 A에 1Vrms(+2.2dBu)를 넘는 신호를 넣지 않는다(Global Constraints).

- [ ] **Step 3: 업로드와 콘솔 확인**

Seed3를 USB-C로 PC에 연결한다(1단계는 USB 전원만, VIN은 비운다). BOOT를 누른 채 RESET을 눌렀다 떼면 DFU 모드다.

Run: `cd effector && make program-dfu`
Expected: dfu-util이 `File downloaded successfully`를 찍고 Seed3가 재시작한다. `screen /dev/ttyACM0`로 붙으면 `effector stage 1 ready. type status`, `status` 입력 시 `gainDb=0.00 ... meter=0x00 ...` 형식 한 줄

- [ ] **Step 4: 첫 소리**

1. 스칼렛 라인아웃 L → TS 케이블 → 잭 A. 잭 B → TS 케이블 → 스칼렛 입력 2(LINE 모드, 게인 최소)
2. Audacity에서 1kHz, 진폭 0.05(약 -26dBFS 피크) 사인을 10초 재생하면서 입력 2를 녹음한다
3. 콘솔에서 `effect 0`을 입력한다

Expected: 녹음에 1kHz가 보이고, `status`의 `peakDbfs`가 사인 재생 중 올라간다. 소리가 없으면 Step 2 배선과 `dmesg`의 CDC 인식(장치는 뜨는데 소리만 없는지)을 먼저 확인한다

- [ ] **Step 5: 콘솔 DFU 진입**

콘솔에서 `dfu`를 입력한다. 장치가 DFU로 다시 열거되면(`dfu-util -l`에 0483:df11) `make program-dfu`로 다시 올린다. 설계서 T14의 업로드 항목을 여기서 미리 확인한다.

---

### Task 9: 1단계 측정

**Files:**
- Create: `effector/measurements/stage1.md`
- Modify: `devices/bass-effector-design.md` (T7 미달 시 대응 판단이 필요할 때만, 사용자 확인 뒤)

**Interfaces:**
- Consumes: Task 3 `static_gain`, Task 7 시리얼 명령, Task 8 결선

측정 규칙:
- 레벨은 Audacity `분석 > 대비(Contrast)`의 RMS 값으로 읽는다. dBFS는 RMS 기준이다
- 한 조건을 3번 재서 모두 기록하고 중앙값으로 판정한다
- 스칼렛 게인 노브 위치를 조건마다 적는다

- [ ] **Step 1: 기록 파일 작성**

`effector/measurements/stage1.md`:

```markdown
# 이펙터 1단계 측정 기록

## 조건

| 항목 | 값 |
|---|---|
| 펌웨어 커밋 | |
| Seed3 개체(구매일) | |
| 스칼렛 입력 모드·게인 | |
| 브레드보드 배선 길이(cm) | |

## M1 보유 베이스 피크

| 연주 | 입력 모드 | 피크 dBFS | 환산 피크 V | 베이스 볼륨·톤 |
|---|---|---|---|---|

## M2 스칼렛 교정

| 항목 | 1회 | 2회 | 3회 | 중앙값 |
|---|---|---|---|---|
| 출력 상수 K_out (dBu - dBFS) | | | | |
| 입력 상수 K_in (dBu - dBFS) | | | | |

## T5 입력 풀스케일 (모듈 단독)

| Input 노브 | 기대 dBu | 측정 dBu | 차이 dB | 판정 |
|---|---|---|---|---|

## T7 바닥 노이즈

| 조건 | Input | 125~8000Hz 다이내믹 레인지 dB | 20Hz~20kHz dB | 판정 |
|---|---|---|---|---|

## T8 왕복 지연

| 경로 | 1회 ms | 2회 ms | 3회 ms | 중앙값 |
|---|---|---|---|---|

## T9 실기기 게인

| 톤 dBFS | static_gain dB | status gainDb | 차이 dB | 판정 |
|---|---|---|---|---|

## T15 클립 표시

| 조건 | 관찰한 meter 값 | 판정 |
|---|---|---|

## 판정 요약
```

- [ ] **Step 2: M2 스칼렛 교정**

1. 스칼렛 라인아웃 L에 TS 케이블만 꽂고 반대쪽 팁·슬리브에 Fluke 101 AC 전압 프로브를 댄다
2. Audacity에서 100Hz, 진폭 0.5 사인을 재생하고 Fluke 값을 읽는다. 100Hz는 Fluke 101 AC 측정 대역 40~500Hz 안이다(설계서 7.3절 M2)
3. `K_out = 20·log10(Vrms / 0.775) - (재생 RMS dBFS)`. 진폭 0.5 사인의 RMS는 -9.03dBFS다
4. TS 케이블로 라인아웃 L을 입력 2(LINE, 게인 최소)에 루프백하고 같은 사인을 녹음해 RMS dBFS를 읽는다. `K_in = (재생 RMS dBFS + K_out) - (녹음 RMS dBFS)`

Expected: 기록 파일 M2 표 3회분. 이후 스칼렛 출력으로 Seed3에 넣는 레벨은 `K_out`으로 계산해 +2.2dBu 이하로만 쓴다

- [ ] **Step 3: T5 입력 풀스케일 (모듈 단독)**

1. 콘솔 `effect 0`, `input 0`
2. 1kHz 사인을 -40dBFS부터 0.5dB씩 올리며 `status`의 `peakDbfs`가 -0.1 이상이 되는 첫 재생 레벨을 찾는다. 그 레벨의 입력 dBu = 재생 RMS dBFS + K_out. 모듈 풀스케일 1Vrms 사인이 +2.2dBu라 같은 RMS 기준으로 비교한다
3. `input 1`(+30dB)로 바꿔 2를 반복한다. `peakDbfs`는 ADC 값이라 같은 레벨에서 넘어야 한다
4. `input 0`으로 되돌린다

판정: 기대값 +2.2dBu ±1dB(설계서 T5의 모듈 단독 기준). Input 위치에 따라 0.2dB 이상 다르면 프리게인이 ADC 앞으로 새는 것이므로 펌웨어를 본다. 패드 비율은 2단계에서 잰다

- [ ] **Step 4: T7 바닥 노이즈**

1. 스칼렛 입력 2 게인을 올려 루프백 케이블만 연결한 상태의 자체 바닥을 10초 녹음한다(값 N_scarlett, 게인 위치 기록)
2. 같은 게인에서 잭 A에 팁·슬리브를 단락한 플러그를 꽂고, 콘솔 `effect 0`에서 `input 0`과 `input 1` 각각 잭 B 출력을 10초 녹음한다(값 N_total)
3. 각 녹음을 Audacity 고역 통과 125Hz와 저역 통과 8000Hz 필터(48dB/oct)를 건 것과 안 건 것 두 가지로 RMS를 읽는다
4. 모듈 노이즈 `N_module = 10·log10(10^(N_total/10) - 10^(N_scarlett/10))` (dBFS를 K_in으로 dBu 환산한 뒤 계산). `input 1` 녹음은 30dB를 빼서 입력 기준으로 맞춘다
5. 다이내믹 레인지 = 입력 풀스케일 dBu(T5 측정값) - N_module dBu
6. 단락 플러그 대신 베이스를 연결하고 줄을 손으로 뮤트한 조건으로 2~5를 반복한다

판정: 125~8000Hz 다이내믹 레인지 100dB 이상(설계서 4.4절). `input 0`과 `input 1`의 입력 기준 값이 같아야 한다. 미달이면 사용자에게 보고하고 배선·접지를 먼저 본다. N_total이 N_scarlett보다 3dB 이상 크지 않으면 스칼렛 게인을 더 올려 다시 잰다

- [ ] **Step 5: T8 왕복 지연**

1. Audacity에서 1초 간격 클릭 트랙(짧은 펄스 5개)을 만든다
2. 스칼렛 라인아웃 L → 입력 2 루프백 케이블로 재생·녹음하고 첫 클릭 시작 샘플 위치를 기록한다
3. 같은 설정으로 잭 A → Seed3 → 잭 B 경로(콘솔 `effect 1`, `blend 1`, `ratio 0`)를 재생·녹음하고 첫 클릭 위치를 기록한다
4. 지연 ms = (Seed3 경로 위치 - 루프백 위치) / 48000 × 1000 (Audacity 프로젝트 레이트를 48000Hz로 맞춘다)

Expected: 기록만 한다. 판정은 T16이다

- [ ] **Step 6: T9 실기기 게인**

1. 콘솔 `input 0`, `threshold -20`, `ratio 0.4628`(20^0.4628 ≈ 4.0), `attrel 1`(어택 20ms, 릴리즈 50ms), `blend 1`, `effect 1`
2. 톤 레벨 -30, -20, -10, -6dBFS마다 `tone 1000 <레벨>`을 입력하고 3초 뒤 `status`를 5번 읽어 `gainDb` 평균을 낸다
3. PC에서 같은 설정의 대조값을 계산한다: `./effector/build/static_gain -20 4.0 0.02 0.05 <레벨>`
4. `tone 0`으로 끈다

판정: 각 레벨 차이 0.1dB 이하. 내부 톤은 ADC 앞에서 넣지 않고 블록 입력을 대체하므로 모듈 경로를 거치지 않는다

- [ ] **Step 7: T15 클립 표시**

1. `tone 1000 0`(피크 0dBFS)을 입력하고 0.1초 안팎 간격으로 `status`를 10번 입력한다
2. `tone 1000 -30`으로 낮추고 1초 뒤 `status`를 입력한다

판정: 1에서 `meter=0xFF`와 `meter=0x00`이 섞여 나오고, 2에서 클립 점멸이 멈춘 값(감쇠량에 따른 비트)이 나온다

- [ ] **Step 8: 판정 요약과 커밋**

기록 파일 판정 요약 절에 M2, T5, T7, T8, T9, T15 결과를 한 줄씩 적는다. T7 미달이나 T8 지연이 연주에 걸릴 수준이면 2단계로 넘어가기 전에 사용자에게 보고한다.

```bash
git add effector/measurements/stage1.md
git commit -m "[Docs] 이펙터 1단계 측정 결과"
```
