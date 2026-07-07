#pragma once

#include <stdint.h>

struct MachineState;

// backend(FastAccelStepper)の現在step値とMachineStateの同期基準。
struct MotionSyncReference {
  int32_t backend_a_steps = 0;
  int32_t backend_b_steps = 0;
  int32_t machine_a_steps = 0;
  int32_t machine_b_steps = 0;
  float machine_x_mm = 0.0f;
  float machine_y_mm = 0.0f;
};

struct MotionDrift {
  int32_t drift_a_steps = 0;
  int32_t drift_b_steps = 0;
  int32_t machine_delta_a_steps = 0;
  int32_t machine_delta_b_steps = 0;
  int32_t backend_delta_a_steps = 0;
  int32_t backend_delta_b_steps = 0;

  bool detected() const { return drift_a_steps != 0 || drift_b_steps != 0; }
};

// MachineStateとstepper backendのstep位置の同期・drift検出を行う純ロジック。
// backendやRTOSへ依存せず、step値は呼び出し側が渡す(nativeテスト可能)。
class MotionSyncTracker {
 public:
  // 現在のbackend step値とMachineStateから同期基準を取る。
  static MotionSyncReference capture(int32_t backend_a_steps,
                                     int32_t backend_b_steps,
                                     const MachineState& machine);

  // 基準からのbackend差分でMachineStateのA/B stepとX/Y概算位置を更新する。
  static void updateEstimate(const MotionSyncReference& reference,
                             int32_t backend_a_steps, int32_t backend_b_steps,
                             float steps_per_mm, MachineState& machine);

  // drift監視の原点を現在値へ揃える(HOME/ZERO/JOB_BEGIN時)。
  void resetReference(int32_t backend_a_steps, int32_t backend_b_steps,
                      const MachineState& machine);

  // 原点からの相対A/B stepをMachineStateとbackendで比較する。
  MotionDrift computeDrift(int32_t backend_a_steps, int32_t backend_b_steps,
                           const MachineState& machine) const;

 private:
  int32_t drift_backend_origin_a_steps_ = 0;
  int32_t drift_backend_origin_b_steps_ = 0;
  int32_t drift_machine_origin_a_steps_ = 0;
  int32_t drift_machine_origin_b_steps_ = 0;
};
