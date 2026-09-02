// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "RingBuffer.hpp"

namespace ProxyAudio::Test {
namespace {

TEST(RingBufferTest, EmptyFullAndPartialOperationsRespectCapacity) {
  RingBuffer<int> buffer(4);
  int output[6] = {-1, -1, -1, -1, -1, -1};
  const int input[6] = {1, 2, 3, 4, 5, 6};

  EXPECT_EQ(buffer.GetCapacity(), 4u);
  EXPECT_EQ(buffer.GetCount(), 0u);
  EXPECT_EQ(buffer.GetAvailableToWrite(), 4u);
  EXPECT_EQ(buffer.Read(output, 2), 0u);
  EXPECT_EQ(buffer.Skip(1), 0u);

  EXPECT_EQ(buffer.Write(input, 6), 4u);
  EXPECT_EQ(buffer.GetCount(), 4u);
  EXPECT_EQ(buffer.GetAvailableToWrite(), 0u);
  EXPECT_EQ(buffer.Write(input + 4, 2), 0u);

  EXPECT_EQ(buffer.Read(output, 6), 4u);
  EXPECT_EQ(std::vector<int>(output, output + 4),
            (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(buffer.GetCount(), 0u);
}

TEST(RingBufferTest, WraparoundSkipAndResetPreserveOrdering) {
  RingBuffer<int> buffer(5);
  const int first[] = {1, 2, 3, 4};
  const int second[] = {5, 6, 7, 8};
  int output[5] = {};

  ASSERT_EQ(buffer.Write(first, 4), 4u);
  ASSERT_EQ(buffer.Skip(3), 3u);
  ASSERT_EQ(buffer.Write(second, 4), 4u);
  ASSERT_EQ(buffer.Read(output, 5), 5u);
  EXPECT_EQ(std::vector<int>(output, output + 5),
            (std::vector<int>{4, 5, 6, 7, 8}));

  ASSERT_EQ(buffer.Write(first, 4), 4u);
  buffer.Reset();
  EXPECT_EQ(buffer.GetCount(), 0u);
  EXPECT_EQ(buffer.GetAvailableToWrite(), 5u);
  EXPECT_EQ(buffer.Read(output, 1), 0u);
}

TEST(RingBufferTest, ZeroCapacityAlwaysReportsUnderrunAndOverrun) {
  RingBuffer<int> buffer(0);
  const int value = 7;
  int output = 0;

  EXPECT_EQ(buffer.GetCapacity(), 0u);
  EXPECT_EQ(buffer.Write(&value, 1), 0u);
  EXPECT_EQ(buffer.Read(&output, 1), 0u);
}

TEST(RingBufferTest, SingleProducerSingleConsumerStressIsLossless) {
  constexpr int kValueCount = 50000;
  RingBuffer<int> buffer(257);
  std::atomic<bool> producerDone{false};
  std::atomic<bool> mismatch{false};

  std::thread producer([&]() {
    for (int value = 0; value < kValueCount;) {
      if (buffer.Write(&value, 1) == 1) {
        ++value;
      } else {
        std::this_thread::yield();
      }
    }
    producerDone.store(true, std::memory_order_release);
  });

  int expected = 0;
  while (expected < kValueCount) {
    int value = -1;
    if (buffer.Read(&value, 1) == 1) {
      if (value != expected) {
        mismatch.store(true, std::memory_order_relaxed);
      }
      ++expected;
    } else {
      EXPECT_FALSE(producerDone.load(std::memory_order_acquire) &&
                   buffer.GetCount() == 0)
          << "producer completed before all values were observed";
      std::this_thread::yield();
    }
  }

  producer.join();
  EXPECT_FALSE(mismatch.load());
  EXPECT_EQ(buffer.GetCount(), 0u);
}

}  // namespace
}  // namespace ProxyAudio::Test
