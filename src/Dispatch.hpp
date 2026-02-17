// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <dispatch/dispatch.h>

namespace ProxyAudio {

struct DispatchTime {
  static DispatchTime Now() {
    return DispatchTime(dispatch_time(DISPATCH_TIME_NOW, 0));
  }

  static DispatchTime Seconds(float seconds) {
    return DispatchTime(dispatch_time(
        DISPATCH_TIME_NOW, static_cast<int64_t>(seconds * NSEC_PER_SEC)));
  }

  dispatch_time_t Get() const { return time_; }

 private:
  explicit DispatchTime(dispatch_time_t time) : time_(time) {}

  dispatch_time_t time_;
};

/*!
 * @function dispatch_async
 *
 * @abstract
 * Submits a block for asynchronous execution on a dispatch queue.
 *
 * @discussion
 * The dispatch_async() function is the fundamental mechanism for submitting
 * blocks to a dispatch queue.
 *
 * Calls to dispatch_async() always return immediately after the block has
 * been submitted, and never wait for the block to be invoked.
 *
 * The target queue determines whether the block will be invoked serially or
 * concurrently with respect to other blocks submitted to that same queue.
 * Serial queues are processed concurrently with respect to each other.
 */
void DispatchAsync(dispatch_block_t block);

/*!
 * @function dispatch_after
 *
 * @abstract
 * Schedule a block for execution on a given queue at a specified time.
 *
 * @discussion
 * Passing DISPATCH_TIME_NOW as the "when" parameter is supported, but not as
 * optimal as calling dispatch_async() instead. Passing DISPATCH_TIME_FOREVER
 * is undefined.
 */
void DispatchAfter(dispatch_block_t block, const DispatchTime& time);

}  // namespace ProxyAudio
