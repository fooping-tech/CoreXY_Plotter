// MotionSyncTracker(backend/MachineState同期とdrift検出)のテスト。
#include <unity.h>

#include "MachineState.h"
#include "MotionSyncTracker.h"
#include "PlotterConfig.h"

void setUp() {}
void tearDown() {}

namespace {

void testCaptureTakesSnapshot() {
  MachineState machine;
  machine.x_mm = 1.5f;
  machine.y_mm = -2.25f;
  machine.a_steps = 240;
  machine.b_steps = -160;

  const MotionSyncReference reference =
      MotionSyncTracker::capture(100, 50, machine);
  TEST_ASSERT_EQUAL_INT32(100, reference.backend_a_steps);
  TEST_ASSERT_EQUAL_INT32(50, reference.backend_b_steps);
  TEST_ASSERT_EQUAL_INT32(240, reference.machine_a_steps);
  TEST_ASSERT_EQUAL_INT32(-160, reference.machine_b_steps);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, reference.machine_x_mm);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -2.25f, reference.machine_y_mm);
}

void testUpdateEstimateAppliesBackendDelta() {
  MachineState machine;
  machine.x_mm = 1.0f;
  machine.y_mm = 2.0f;
  machine.a_steps = 240;
  machine.b_steps = -160;
  const MotionSyncReference reference =
      MotionSyncTracker::capture(100, 50, machine);

  // backend A +80 / B -80 は CoreXYで(dx=0mm, dy=+1mm)に相当する(80 steps/mm)。
  MotionSyncTracker::updateEstimate(reference, 180, -30, STEPS_PER_MM, machine);
  TEST_ASSERT_EQUAL_INT32(320, machine.a_steps);
  TEST_ASSERT_EQUAL_INT32(-240, machine.b_steps);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, machine.x_mm);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.0f, machine.y_mm);
}

void testUpdateEstimateWithNoDeltaKeepsState() {
  MachineState machine;
  machine.x_mm = 5.0f;
  machine.y_mm = 7.0f;
  machine.a_steps = 960;
  machine.b_steps = -160;
  const MotionSyncReference reference =
      MotionSyncTracker::capture(-20, 30, machine);
  MotionSyncTracker::updateEstimate(reference, -20, 30, STEPS_PER_MM, machine);
  TEST_ASSERT_EQUAL_INT32(960, machine.a_steps);
  TEST_ASSERT_EQUAL_INT32(-160, machine.b_steps);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 5.0f, machine.x_mm);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 7.0f, machine.y_mm);
}

void testDriftZeroWhenMachineFollowsBackend() {
  MachineState machine;
  machine.a_steps = 100;
  machine.b_steps = 200;
  MotionSyncTracker tracker;
  tracker.resetReference(1000, 2000, machine);

  machine.a_steps += 80;
  machine.b_steps -= 80;
  const MotionDrift drift = tracker.computeDrift(1080, 1920, machine);
  TEST_ASSERT_FALSE(drift.detected());
  TEST_ASSERT_EQUAL_INT32(80, drift.machine_delta_a_steps);
  TEST_ASSERT_EQUAL_INT32(-80, drift.machine_delta_b_steps);
  TEST_ASSERT_EQUAL_INT32(80, drift.backend_delta_a_steps);
  TEST_ASSERT_EQUAL_INT32(-80, drift.backend_delta_b_steps);
}

void testDriftDetectedWhenBackendDiverges() {
  MachineState machine;
  machine.a_steps = 0;
  machine.b_steps = 0;
  MotionSyncTracker tracker;
  tracker.resetReference(0, 0, machine);

  // MachineStateは+160/+160のつもりだが、backendは+150/+165しか動いていない。
  machine.a_steps = 160;
  machine.b_steps = 160;
  const MotionDrift drift = tracker.computeDrift(150, 165, machine);
  TEST_ASSERT_TRUE(drift.detected());
  TEST_ASSERT_EQUAL_INT32(10, drift.drift_a_steps);
  TEST_ASSERT_EQUAL_INT32(-5, drift.drift_b_steps);
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testCaptureTakesSnapshot);
  RUN_TEST(testUpdateEstimateAppliesBackendDelta);
  RUN_TEST(testUpdateEstimateWithNoDeltaKeepsState);
  RUN_TEST(testDriftZeroWhenMachineFollowsBackend);
  RUN_TEST(testDriftDetectedWhenBackendDiverges);
  return UNITY_END();
}
