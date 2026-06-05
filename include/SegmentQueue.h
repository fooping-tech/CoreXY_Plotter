#pragma once

#include <stddef.h>
#include "SegmentGenerator.h"

class SegmentQueue {
 public:
  static constexpr size_t CAPACITY = 512;

  bool enqueue(const MotionSegment& segment);
  bool dequeue(MotionSegment& segment);
  const MotionSegment* peekNext() const;
  void clear();
  bool isEmpty() const;
  bool isFull() const;
  size_t count() const;

 private:
  MotionSegment segments_[CAPACITY];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
};
