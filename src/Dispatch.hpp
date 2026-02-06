// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <dispatch/dispatch.h>

namespace ProxyAudio {

//
// @function dispatch_async
//
// @abstract
// Submits a block for asynchronous execution on a dispatch queue.
//
// @discussion
// The dispatch_async() function is the fundamental mechanism for submitting
// blocks to a dispatch queue.
//
// Calls to dispatch_async() always return immediately after the block has
// been submitted, and never wait for the block to be invoked.
//
// The target queue determines whether the block will be invoked serially or
// concurrently with respect to other blocks submitted to that same queue.
// Serial queues are processed concurrently with respect to each other.
//
void DispatchAsync(dispatch_block_t block);

}  // namespace ProxyAudio
