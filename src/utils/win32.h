#pragma once
#include <WTypesbase.h>

#include <bit>
#include <execution>

namespace utils::win32 {
constexpr UINT16 CreateHighColor(UINT red, UINT green, UINT blue) {
  static_assert(std::endian::native == std::endian::little);
  return static_cast<UINT16>(((red >> 3U) << 10U) | ((green >> 3U) << 5U) |
                             (blue >> 3U));
}

bool IsKeyPressed(INT key);

LONGLONG GetMilliseconds();
void LimitFrameRate(INT fps, LONGLONG start_time);
HWND CreateMainWindow(HINSTANCE instance, LONG width, LONG height);
}  // namespace utils::win32
