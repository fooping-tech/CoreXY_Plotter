#include "SegmentQueue.h"

bool SegmentQueue::enqueue(const MotionSegment& segment) {
  if (isFull()) return false;
  segments_[tail_] = segment;
  tail_ = (tail_ + 1) % CAPACITY;
  ++count_;
  return true;
}

bool SegmentQueue::dequeue(MotionSegment& segment) {
  if (isEmpty()) return false;
  segment = segments_[head_];
  head_ = (head_ + 1) % CAPACITY;
  --count_;
  return true;
}

const MotionSegment* SegmentQueue::peekNext() const {
  if (isEmpty()) return nullptr;
  return &segments_[head_];
}

void SegmentQueue::clear() {
  head_ = 0;
  tail_ = 0;
  count_ = 0;
}

bool SegmentQueue::isEmpty() const { return count_ == 0; }
bool SegmentQueue::isFull() const { return count_ == CAPACITY; }
size_t SegmentQueue::count() const { return count_; }
