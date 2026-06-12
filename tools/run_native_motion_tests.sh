#!/bin/sh
set -eu

c++ -std=c++17 -DSIMULATION_MODE=1 \
  -Iinclude -Itest/native \
  test/native/test_motion_drift.cpp \
  src/CoreXYKinematics.cpp src/SegmentGenerator.cpp src/SegmentQueue.cpp \
  -o /tmp/corexy_plotter_native_motion_tests

/tmp/corexy_plotter_native_motion_tests
