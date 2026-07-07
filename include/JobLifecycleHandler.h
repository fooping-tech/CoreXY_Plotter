#pragma once

#include "JobController.h"
#include "MachineState.h"

class HomingController;
class MotorMelodyController;
class PenController;
class SafetyManager;
class StepperBackendFastAccel;
class TMC2209Manager;
class XYMotionPlanner;

// JOB_BEGIN前の自動homingとJOB_END終了処理(pen up→park→ジングル)を
// JobControllerの状態機械へ接続するグルー。motionTaskから呼ばれる。
class JobLifecycleHandler {
 public:
  using PreflightProvider = JobPreflight (*)();

  JobLifecycleHandler(JobController& job, SafetyManager& safety,
                      MachineState& machine, TMC2209Manager& tmc,
                      HomingController& homing, PenController& pen,
                      MotorMelodyController& melody,
                      StepperBackendFastAccel& backend, XYMotionPlanner& xy,
                      PreflightProvider current_preflight)
      : job_(job),
        safety_(safety),
        machine_(machine),
        tmc_(tmc),
        homing_(homing),
        pen_(pen),
        melody_(melody),
        backend_(backend),
        xy_(xy),
        current_preflight_(current_preflight) {}

  // JOB_BEGIN_AUTO_HOME有効時、未homedならTMC初期化+HOME相当を実行する。
  // falseはJOB_BEGINを拒否すべき失敗。trueは続行(条件不成立で何もしない場合を含む)。
  bool prepareJobBeginAutoHome();

  // JOB_END処理。pen up→park移動→終了ジングル→JobController::endJob。
  void handleJobEnd();

 private:
  bool moveToJobEndPark();
  static bool preflightIdle(const JobPreflight& preflight);

  JobController& job_;
  SafetyManager& safety_;
  MachineState& machine_;
  TMC2209Manager& tmc_;
  HomingController& homing_;
  PenController& pen_;
  MotorMelodyController& melody_;
  StepperBackendFastAccel& backend_;
  XYMotionPlanner& xy_;
  PreflightProvider current_preflight_;
};
