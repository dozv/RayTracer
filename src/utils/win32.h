#pragma once
#include <WTypesbase.h>

#include <execution>

namespace utils::win32 {
constexpr UINT16 CreateHighColor(UINT8 red, UINT8 green, UINT8 blue) {
  static_assert(std::endian::native == std::endian::little);
  return ((red >> 3U) << 10U) | ((green >> 3U) << 5U) | (blue >> 3U);
}

bool IsKeyPressed(INT key);

LONGLONG GetMilliseconds();
void LimitFrameRate(INT fps, LONGLONG start_time);
HWND CreateMainWindow(HINSTANCE instance, LONG width, LONG height);
}  // namespace utils::win32
