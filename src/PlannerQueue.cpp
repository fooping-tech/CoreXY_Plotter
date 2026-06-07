#include "PlannerQueue.h"

bool PlannerQueue::enqueue(const MotionBlock& block) {
  if (isFull()) return false;
  blocks_[tail_] = block;
  tail_ = (tail_ + 1) % CAPACITY;
  ++count_;
  return true;
}

bool PlannerQueue::dequeue(MotionBlock& block) {
  if (isEmpty()) return false;
  block = blocks_[head_];
  head_ = (head_ + 1) % CAPACITY;
  --count_;
  return true;
}

const MotionBlock* PlannerQueue::peekNext() const {
  return isEmpty() ? nullptr : &blocks_[head_];
}

MotionBlock* PlannerQueue::at(size_t index) {
  if (index >= count_) return nullptr;
  return &blocks_[(head_ + index) % CAPACITY];
}

const MotionBlock* PlannerQueue::at(size_t index) const {
  if (index >= count_) return nullptr;
  return &blocks_[(head_ + index) % CAPACITY];
}

void PlannerQueue::clear() {
  head_ = 0;
  tail_ = 0;
  count_ = 0;
}

bool PlannerQueue::isEmpty() const { return count_ == 0; }
bool PlannerQueue::isFull() const { return count_ == CAPACITY; }
size_t PlannerQueue::count() const { return count_; }
