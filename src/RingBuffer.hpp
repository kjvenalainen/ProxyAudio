// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace ProxyAudio {

// Thread safe single-producer, single-consumer ring buffer without locking.
template <typename T>
class RingBuffer {
 public:
  RingBuffer(size_t capacity)
      : buffer_(capacity + 1), writeIdx_(0), readIdx_(0) {}

  ~RingBuffer() = default;

  // Get the current number of values in the buffer.
  size_t GetCount() const {
    // Make sure to handle the wrap-around case.
    const auto writeIdx = writeIdx_.load(std::memory_order_acquire);
    const auto readIdx = readIdx_.load(std::memory_order_acquire);
    return (writeIdx - readIdx + buffer_.size()) % buffer_.size();
  }

  // Maximum number of values that can be stored.
  size_t GetCapacity() const { return buffer_.size() - 1; }

  // Get the number of values that can be written to the buffer.
  size_t GetAvailableToWrite() const {
    const auto writeIdx = writeIdx_.load(std::memory_order_acquire);
    const auto readIdx = readIdx_.load(std::memory_order_acquire);
    const size_t size = buffer_.size();
    return (readIdx - writeIdx - 1 + size) % size;
  }

  // Write an array of values to the buffer. Returns the number of values
  // actually written (may be less than count if the buffer is full).
  size_t Write(const T* values, size_t count) {
    const auto writeIdx = writeIdx_.load(std::memory_order_relaxed);
    const auto readIdx = readIdx_.load(std::memory_order_acquire);
    const size_t size = buffer_.size();
    const size_t available = (readIdx - writeIdx - 1 + size) % size;
    count = std::min(count, available);
    if (count == 0) {
      return 0;
    }

    // Number of contiguous slots from writeIdx to end of buffer.
    const size_t toEnd = size - writeIdx;
    if (count <= toEnd) {
      std::copy_n(values, count, buffer_.data() + writeIdx);
    } else {
      // Write wraps around: first fill to end, then from the start.
      std::copy_n(values, toEnd, buffer_.data() + writeIdx);
      std::copy_n(values + toEnd, count - toEnd, buffer_.data());
    }

    writeIdx_.store((writeIdx + count) % size, std::memory_order_release);
    return count;
  }

  // Read an array of values from the buffer into the provided array. Returns
  // the number of values actually read (may be less than count if the buffer
  // doesn't contain enough data).
  size_t Read(T* values, size_t count) {
    const auto readIdx = readIdx_.load(std::memory_order_relaxed);
    const auto writeIdx = writeIdx_.load(std::memory_order_acquire);
    const size_t size = buffer_.size();
    const size_t stored = (writeIdx - readIdx + size) % size;
    count = std::min(count, stored);
    if (count == 0) {
      return 0;
    }

    // Number of contiguous slots from readIdx to end of buffer.
    const size_t toEnd = size - readIdx;
    if (count <= toEnd) {
      std::copy_n(buffer_.data() + readIdx, count, values);
    } else {
      // Read wraps around: first read to end, then from the start.
      std::copy_n(buffer_.data() + readIdx, toEnd, values);
      std::copy_n(buffer_.data(), count - toEnd, values + toEnd);
    }

    readIdx_.store((readIdx + count) % size, std::memory_order_release);
    return count;
  }

 private:
  // Internally the buffer is at least capacity + 1 elements to distinguish
  // between full and empty.
  std::vector<T> buffer_;
  std::atomic<size_t> writeIdx_;
  std::atomic<size_t> readIdx_;
};

}  // namespace ProxyAudio
