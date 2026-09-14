#include "check.h"
#include "pga_controller.h"

namespace {

PgaController::Config testConfig() {
    return {12, -50.0f, 5, -0.1f, 2};
}

void runBlocks(PgaController& controller, int count, float desiredDb, float peakDbfs) {
    for (int block = 0; block < count; ++block) {
        controller.processBlock(desiredDb, peakDbfs);
    }
}

}  // namespace

TEST_CASE("PGA waits for a quiet hold before moving") {
    PgaController controller(testConfig());
    runBlocks(controller, 100, 9.0f, -20.0f);
    CHECK(controller.pgaStep() == 0);
    runBlocks(controller, 4, 9.0f, -60.0f);
    CHECK(controller.pgaStep() == 0);
    runBlocks(controller, 1, 9.0f, -60.0f);
    CHECK(controller.pgaStep() == 6);
    CHECK(controller.takeStepChange());
    CHECK(!controller.takeStepChange());
}

TEST_CASE("PGA never exceeds the maximum step") {
    PgaController controller(testConfig());
    runBlocks(controller, 10, 40.0f, -60.0f);
    CHECK(controller.pgaStep() == 12);
}

TEST_CASE("Clip lowers PGA immediately and caps it until the knob moves") {
    PgaController controller(testConfig());
    runBlocks(controller, 10, 9.0f, -60.0f);
    CHECK(controller.pgaStep() == 6);
    controller.processBlock(9.0f, 0.0f);
    CHECK(controller.pgaStep() == 5);
    runBlocks(controller, 20, 9.0f, -60.0f);
    CHECK(controller.pgaStep() == 5);
    runBlocks(controller, 10, 7.5f, -60.0f);
    CHECK(controller.pgaStep() == 5);
    runBlocks(controller, 10, 10.5f, -60.0f);
    CHECK(controller.pgaStep() == 7);
}

TEST_CASE("Compensation follows the step after the alignment delay") {
    PgaController controller(testConfig());
    runBlocks(controller, 10, 9.0f, -60.0f);
    runBlocks(controller, 3, 9.0f, -60.0f);
    CHECK_NEAR(controller.compensationDb(), -9.0, 1e-6);

    PgaController delayed(testConfig());
    runBlocks(delayed, 4, 9.0f, -60.0f);
    delayed.processBlock(9.0f, -60.0f);
    CHECK(delayed.pgaStep() == 6);
    CHECK_NEAR(delayed.compensationDb(), 0.0, 1e-6);
    delayed.processBlock(9.0f, -60.0f);
    CHECK_NEAR(delayed.compensationDb(), 0.0, 1e-6);
    delayed.processBlock(9.0f, -60.0f);
    CHECK_NEAR(delayed.compensationDb(), -9.0, 1e-6);
}
