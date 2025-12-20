// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <os/log.h>

#include <span>

namespace ProxyAudio {

// Gets the filename from a file path.
constexpr static inline const char* GetFilename(const char* filePath) {
  size_t lastSlash = 0;
  for (size_t i = 0; filePath[i] != '\0'; i++) {
    if (filePath[i] == '/') {
      lastSlash = i;
    }
  }

  if (lastSlash == 0) {
    return filePath;
  }

  return filePath + lastSlash + 1;
}

// Converts a pointer and size into a span, clamping the size to the size of the
// array.
template <typename T>
constexpr static std::span<T> SafeSpan(T* out, size_t outSize) {
  return std::span<T>(out, outSize / sizeof(T));
}

// Identity converter for when T == U (no conversion needed)
struct IdentityConverter {
  template <typename T>
  constexpr T operator()(const T& value) const {
    return value;
  }
};

// Copies a span to another span, clamping the size to the size of the
// destination span and returning the number of items copied.
// DataConverter must be a callable type (function pointer, lambda, functor,
// etc.) that can be called as converter(in[i]) and returns a type convertible
// to T.
template <typename T, typename U, typename DataConverter = IdentityConverter>
constexpr static size_t SafeCopyToSpan(std::span<T> out,
                                       const std::span<U> in,
                                       const DataConverter& converter = {}) {
  const size_t copySize = std::min(out.size(), in.size());
  for (size_t i = 0; i < copySize; ++i) {
    out[i] = converter(in[i]);
  }
  return copySize;
}

// Overload that accepts containers (like vectors) that can be converted to
// spans.
template <typename T,
          typename Container,
          typename DataConverter = IdentityConverter>
  requires requires(const Container& c) {
    std::span<const typename Container::value_type>(c);
  }
constexpr static size_t SafeCopyToSpan(std::span<T> out,
                                       const Container& in,
                                       const DataConverter& converter = {}) {
  const std::span<const typename Container::value_type> inSpan(in);
  return SafeCopyToSpan(out, inSpan, converter);
}

#define Log(inFormat, ...)                                 \
  os_log(OS_LOG_DEFAULT, "%{public}s:%d | " inFormat "\n", \
         ProxyAudio::GetFilename(__FILE__), __LINE__, ##__VA_ARGS__)

}  // namespace ProxyAudio
