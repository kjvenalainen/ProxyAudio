// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <sys/syslog.h>

#include <aspl/Tracer.hpp>
#include <cstdarg>
#include <memory>
#include <sstream>
#include <string>

namespace ProxyAudio {

struct Tracer : public aspl::Tracer {
  static constexpr size_t MaxMessageLen = 1024;

  struct ThreadLocalState {
    UInt32 DepthCounter = 0;
    UInt32 IgnoreCounter = 0;
  };

  static unsigned long GetThreadID() {
    UInt64 tid = 0;
    pthread_threadid_np(nullptr, &tid);

    return static_cast<unsigned long>(tid);
  }

  static void* CreateThreadLocalState() { return new ThreadLocalState; }

  static void DestroyThreadLocalState(void* ptr) {
    delete static_cast<ThreadLocalState*>(ptr);
  }

  ThreadLocalState& GetThreadLocalState() {
    void* ptr = pthread_getspecific(threadKey_);

    if (!ptr) {
      ptr = CreateThreadLocalState();
      pthread_setspecific(threadKey_, ptr);
    }

    return *static_cast<ThreadLocalState*>(ptr);
  }

 public:
  enum Level {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
  };

  static constexpr size_t DepthSoftLimit = 10;

  explicit Tracer(Mode mode = Mode::Syslog,
                  Style style = Style::Hierarchical,
                  Level level = Info)
      : aspl::Tracer(mode, style), mode_(mode), level_(level) {
    pthread_key_create(&threadKey_, DestroyThreadLocalState);
  }

  // Ensure direct visibility of the basic Message (at debug level).
  using aspl::Tracer::Message;

  // Log a message with level.
  void Message(const Level level, const char* format, ...) {
    if (mode_ == Mode::Noop) {
      return;
    }

    auto& threadState = GetThreadLocalState();

    if (threadState.IgnoreCounter != 0 &&
        threadState.DepthCounter >= threadState.IgnoreCounter) {
      return;
    }

    char message[MaxMessageLen] = {};

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const auto str = FormatMessage(message, threadState.DepthCounter);

    Print(level, str.c_str());
  }

  // Base implementation, prints all messages at Debug level.
  void Print(const char* message) override { Print(Debug, message); }

  // Print with a configurable log level.
  void Print(const Level level, const char* message) {
    if (level >= level_) {
      switch (mode_) {
        case Mode::Noop:
          return;

        case Mode::Stderr:
          fprintf(stderr, "[PA] %s\n", message);
          return;

        case Mode::Syslog:
          syslog(LOG_NOTICE, "[PA] [tid:%lu] %s", GetThreadID(), message);
          return;

        case Mode::Custom:
          return;
      }
    }
  }

  static std::shared_ptr<ProxyAudio::Tracer> FromTracer(
      std::shared_ptr<aspl::Tracer> parent) {
    return std::static_pointer_cast<ProxyAudio::Tracer>(parent);
  }

 private:
  const Mode mode_;
  Level level_;
  pthread_key_t threadKey_;
};

}  // namespace ProxyAudio
