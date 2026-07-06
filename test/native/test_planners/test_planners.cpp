// TrapezoidPlanner / JunctionPlanner / PlannerQueueの安全網テスト。
// RF1のmotion経路リファクタリングで挙動が変わらないことを検出する。
#include <cmath>

#include <unity.h>

#include "JunctionPlanner.h"
#include "MotionBlock.h"
#include "PlannerQueue.h"
#include "PlotterConfig.h"
#include "TrapezoidPlanner.h"

void setUp() {}
void tearDown() {}

namespace {

MotionBlock makeBlock(float dx_mm, float dy_mm, float feed_mm_min) {
  MotionBlock block{};
  block.dx_mm = dx_mm;
  block.dy_mm = dy_mm;
  block.length_mm = std::sqrt(dx_mm * dx_mm + dy_mm * dy_mm);
  block.nominal_speed_mm_min = feed_mm_min;
  return block;
}

void testTrapezoidLongBlockHasCruisePhase() {
  TrapezoidPlanner planner;
  MotionBlock block = makeBlock(100.0f, 0.0f, 600.0f);
  TEST_ASSERT_TRUE(planner.plan(block));
  TEST_ASSERT_TRUE(block.trapezoid_planned);
  TEST_ASSERT_FALSE(block.triangular_profile);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, block.peak_speed_mm_s);
  TEST_ASSERT_TRUE(block.cruise_distance_mm > 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, block.length_mm,
                           block.acceleration_distance_mm +
                               block.cruise_distance_mm +
                               block.deceleration_distance_mm);
  TEST_ASSERT_TRUE(block.acceleration_time_s > 0.0f);
  TEST_ASSERT_TRUE(block.cruise_time_s > 0.0f);
  TEST_ASSERT_TRUE(block.deceleration_time_s > 0.0f);
}

void testTrapezoidShortBlockBecomesTriangular() {
  TrapezoidPlanner planner;
  // 0.02mmの短moveでは10mm/sの巡航速度に到達できない。
  MotionBlock block = makeBlock(0.02f, 0.0f, 600.0f);
  TEST_ASSERT_TRUE(planner.plan(block));
  TEST_ASSERT_TRUE(block.triangular_profile);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, block.cruise_distance_mm);
  TEST_ASSERT_TRUE(block.peak_speed_mm_s < block.nominal_speed_mm_s + 1e-3f);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, block.length_mm,
                           block.acceleration_distance_mm +
                               block.deceleration_distance_mm);
}

void testTrapezoidRejectsZeroLength() {
  TrapezoidPlanner planner;
  MotionBlock block = makeBlock(0.0f, 0.0f, 600.0f);
  TEST_ASSERT_FALSE(planner.plan(block));
  TEST_ASSERT_FALSE(block.trapezoid_planned);
}

void testTrapezoidClampsFeedToMax() {
  TrapezoidPlanner planner;
  MotionBlock block = makeBlock(50.0f, 0.0f, MAX_FEED_MM_MIN * 4.0f);
  TEST_ASSERT_TRUE(planner.plan(block));
  TEST_ASSERT_FLOAT_WITHIN(1e-2f, MAX_FEED_MM_MIN / 60.0f,
                           block.nominal_speed_mm_s);
}

void testJunctionRejectsEmptyQueue() {
  JunctionPlanner planner;
  PlannerQueue queue;
  TEST_ASSERT_FALSE(planner.plan(queue));
}

void testJunctionStraightPathKeepsSpeed() {
  JunctionPlanner planner;
  PlannerQueue queue;
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(10.0f, 0.0f, 1200.0f)));
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(10.0f, 0.0f, 1200.0f)));
  TEST_ASSERT_TRUE(planner.plan(queue));
  const MotionBlock* first = queue.at(0);
  const MotionBlock* second = queue.at(1);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  // 最初のentryと最後のexitは0、直進junctionはnominal速度を維持する。
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, first->entry_speed_mm_s);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, second->exit_speed_mm_s);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 20.0f, first->exit_speed_mm_s);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, first->exit_speed_mm_s,
                           second->entry_speed_mm_s);
}

void testJunctionRightAngleSlowsDown() {
  JunctionPlanner planner;
  PlannerQueue queue;
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(10.0f, 0.0f, 1200.0f)));
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(0.0f, 10.0f, 1200.0f)));
  TEST_ASSERT_TRUE(planner.plan(queue));
  const MotionBlock* first = queue.at(0);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_TRUE(first->exit_speed_mm_s > 0.0f);
  TEST_ASSERT_TRUE(first->exit_speed_mm_s < first->nominal_speed_mm_s);
}

void testJunctionReversalStops() {
  JunctionPlanner planner;
  PlannerQueue queue;
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(10.0f, 0.0f, 1200.0f)));
  TEST_ASSERT_TRUE(queue.enqueue(makeBlock(-10.0f, 0.0f, 1200.0f)));
  TEST_ASSERT_TRUE(planner.plan(queue));
  const MotionBlock* first = queue.at(0);
  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, first->exit_speed_mm_s);
}

void testPlannerQueueFifoAndCapacity() {
  PlannerQueue queue;
  TEST_ASSERT_TRUE(queue.isEmpty());
  for (size_t index = 0; index < PlannerQueue::CAPACITY; ++index) {
    MotionBlock block = makeBlock(1.0f + static_cast<float>(index), 0.0f, 600.0f);
    TEST_ASSERT_TRUE(queue.enqueue(block));
  }
  TEST_ASSERT_TRUE(queue.isFull());
  MotionBlock overflow = makeBlock(1.0f, 0.0f, 600.0f);
  TEST_ASSERT_FALSE(queue.enqueue(overflow));

  MotionBlock block{};
  TEST_ASSERT_TRUE(queue.dequeue(block));
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, block.dx_mm);
  TEST_ASSERT_EQUAL_UINT32(PlannerQueue::CAPACITY - 1,
                           static_cast<uint32_t>(queue.count()));
  queue.clear();
  TEST_ASSERT_TRUE(queue.isEmpty());
  TEST_ASSERT_NULL(queue.at(0));
}

}  // namespace

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(testTrapezoidLongBlockHasCruisePhase);
  RUN_TEST(testTrapezoidShortBlockBecomesTriangular);
  RUN_TEST(testTrapezoidRejectsZeroLength);
  RUN_TEST(testTrapezoidClampsFeedToMax);
  RUN_TEST(testJunctionRejectsEmptyQueue);
  RUN_TEST(testJunctionStraightPathKeepsSpeed);
  RUN_TEST(testJunctionRightAngleSlowsDown);
  RUN_TEST(testJunctionReversalStops);
  RUN_TEST(testPlannerQueueFifoAndCapacity);
  return UNITY_END();
}
