// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "Dispatch.hpp"

namespace ProxyAudio {

void DispatchAsync(dispatch_block_t block) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                 block);
}

}  // namespace ProxyAudio
