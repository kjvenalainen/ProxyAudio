// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <type_traits>
#include <vector>

namespace ProxyAudio {

template <class T>
struct is_vector : public std::false_type {};

template <class T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : public std::true_type {};

}  // namespace ProxyAudio
