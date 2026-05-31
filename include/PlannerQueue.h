#pragma once

#include <stddef.h>
#include "MotionBlock.h"

class PlannerQueue {
 public:
  static constexpr size_t CAPACITY = 16;
  bool enqueue(const MotionBlock& block);
  bool dequeue(MotionBlock& block);
  const MotionBlock* peekNext() const;
  bool isEmpty() const;
  bool isFull() const;
  size_t count() const;

 private:
  MotionBlock blocks_[CAPACITY];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
};
