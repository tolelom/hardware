#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "effector_processor.h"
#include "serial_console.h"
#include "signal_helpers.h"

namespace {

struct FakeHost : ConsoleHost {
    std::vector<std::string> writes;
    EffectorParameters applied;
    int applyCount = 0;
    float toneFrequencyHz = -1.0f;
    float toneAmplitude = -1.0f;
    int resetCount = 0;

    void write(const char* text) override { writes.emplace_back(text); }
    void applyParameters(const EffectorParameters& parameters) override {
        applied = parameters;
        ++applyCount;
    }
    void setTestTone(float frequencyHz, float amplitude) override {
        toneFrequencyHz = frequencyHz;
        toneAmplitude = amplitude;
    }
    void resetToBootloader() override { ++resetCount; }
};

struct Fixture {
    EffectorProcessor processor{{kTestSampleRateHz, 48}};
    EffectorParameters parameters;
    FakeHost host;
    SerialConsole console{processor, parameters, host, kTestSampleRateHz};

    void feed(const char* text) { console.feed(text, std::strlen(text)); }
};

bool startsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

}  // namespace

TEST_CASE("Console maps a knob command and applies the parameters") {
    Fixture fixture;
    fixture.feed("input 0.5\n");
    CHECK(fixture.host.applyCount == 1);
    CHECK_NEAR(fixture.host.applied.inputDriveDb, 15.0f, 1e-4f);
    CHECK(fixture.host.writes.size() == 1);
    CHECK(startsWith(fixture.host.writes[0], "gainDb="));
}

TEST_CASE("Console prints the command list for an unknown command") {
    Fixture fixture;
    fixture.feed("pga 3\n");
    CHECK(fixture.host.applyCount == 0);
    CHECK(fixture.host.writes.size() == 1);
    CHECK(startsWith(fixture.host.writes[0], "commands:"));
}

TEST_CASE("Console splits lines on CR LF and skips empty lines") {
    Fixture fixture;
    fixture.feed("\r\nstatus\r\n\n");
    CHECK(fixture.host.writes.size() == 1);
    CHECK(startsWith(fixture.host.writes[0], "gainDb="));
    CHECK(fixture.host.applyCount == 0);
}

TEST_CASE("Console handles a line that arrives in pieces") {
    Fixture fixture;
    fixture.feed("out");
    fixture.feed("put 1\n");
    CHECK(fixture.host.applyCount == 1);
    CHECK_NEAR(fixture.host.applied.outputGainDb, 12.0f, 1e-4f);
}

TEST_CASE("Console clamps the sidechain high pass below Nyquist") {
    Fixture fixture;
    fixture.feed("sidechain 40000\n");
    CHECK(fixture.host.applied.compressor.sidechainHighPassHz < kTestSampleRateHz / 2.0f);
}

TEST_CASE("Console forwards the test tone and the bootloader reset to the host") {
    Fixture fixture;
    fixture.feed("tone 1000 -20\n");
    CHECK_NEAR(fixture.host.toneFrequencyHz, 1000.0f, 1e-3f);
    CHECK_NEAR(fixture.host.toneAmplitude, 0.1f, 1e-4f);
    fixture.feed("tone 0\n");
    CHECK_NEAR(fixture.host.toneAmplitude, 0.0f, 1e-6f);
    fixture.feed("dfu\n");
    CHECK(fixture.host.resetCount == 1);
}
