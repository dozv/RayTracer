#pragma once

#include <type_traits>

#include "matrix_view.h"

namespace {
template <typename T>
concept FloatLike = std::is_floating_point_v<T>;

template <FloatLike T>
constexpr float RemapRange(T value, T src_min, T src_max, T dst_min,
                           T dst_max) {
  return ((dst_max - dst_min) / (src_max - src_min)) * (value - src_min) +
         dst_min;
}
}  // namespace
