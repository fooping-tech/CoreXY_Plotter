#include "MotionSyncTracker.h"

#include "CoreXYKinematics.h"
#include "MachineState.h"

MotionSyncReference MotionSyncTracker::capture(int32_t backend_a_steps,
                                               int32_t backend_b_steps,
                                               const MachineState& machine) {
  MotionSyncReference reference{};
  reference.backend_a_steps = backend_a_steps;
  reference.backend_b_steps = backend_b_steps;
  reference.machine_a_steps = machine.a_steps;
  reference.machine_b_steps = machine.b_steps;
  reference.machine_x_mm = machine.x_mm;
  reference.machine_y_mm = machine.y_mm;
  return reference;
}

void MotionSyncTracker::updateEstimate(const MotionSyncReference& reference,
                                       int32_t backend_a_steps,
                                       int32_t backend_b_steps,
                                       float steps_per_mm,
                                       MachineState& machine) {
  const int32_t delta_a_steps = backend_a_steps - reference.backend_a_steps;
  const int32_t delta_b_steps = backend_b_steps - reference.backend_b_steps;
  machine.a_steps = reference.machine_a_steps + delta_a_steps;
  machine.b_steps = reference.machine_b_steps + delta_b_steps;
  const CoreXYXYDeltaMm delta = CoreXYKinematics::abStepsToXYDeltaMm(
      delta_a_steps, delta_b_steps, steps_per_mm);
  machine.x_mm = reference.machine_x_mm + delta.dx_mm;
  machine.y_mm = reference.machine_y_mm + delta.dy_mm;
}

void MotionSyncTracker::resetReference(int32_t backend_a_steps,
                                       int32_t backend_b_steps,
                                       const MachineState& machine) {
  drift_backend_origin_a_steps_ = backend_a_steps;
  drift_backend_origin_b_steps_ = backend_b_steps;
  drift_machine_origin_a_steps_ = machine.a_steps;
  drift_machine_origin_b_steps_ = machine.b_steps;
}

MotionDrift MotionSyncTracker::computeDrift(int32_t backend_a_steps,
                                            int32_t backend_b_steps,
                                            const MachineState& machine) const {
  MotionDrift drift{};
  drift.machine_delta_a_steps =
      machine.a_steps - drift_machine_origin_a_steps_;
  drift.machine_delta_b_steps =
      machine.b_steps - drift_machine_origin_b_steps_;
  drift.backend_delta_a_steps = backend_a_steps - drift_backend_origin_a_steps_;
  drift.backend_delta_b_steps = backend_b_steps - drift_backend_origin_b_steps_;
  drift.drift_a_steps =
      drift.machine_delta_a_steps - drift.backend_delta_a_steps;
  drift.drift_b_steps =
      drift.machine_delta_b_steps - drift.backend_delta_b_steps;
  return drift;
}
