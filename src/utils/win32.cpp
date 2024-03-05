#include "win32.h"

#include <cassert>

namespace {
LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM w_param,
                                    LPARAM l_param) {
  LRESULT result = 0;

  switch (message) {
    case WM_CLOSE: {
      Beep(1000, 100);
      if (MessageBox(window, TEXT("Really quit?"), TEXT("Quit"), MB_OKCANCEL) ==
          IDOK) {
        DestroyWindow(window);
      }
    } break;
    case WM_DESTROY: {
      PostQuitMessage(0);
    } break;
    default: {
      result = DefWindowProc(window, message, w_param, l_param);
    } break;
  }

  return result;
}
}  // namespace

bool utils::win32::IsKeyPressed(INT key) {
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

LONGLONG utils::win32::GetMilliseconds() {
  // Get QPC data.
  LARGE_INTEGER number_of_ticks{};
  LARGE_INTEGER number_of_ticks_per_second{};
  QueryPerformanceFrequency(&number_of_ticks_per_second);
  QueryPerformanceCounter(&number_of_ticks);
  // `T := (N * 1000) / f ms`, where `N` is the number of ticks and `f` is the
  // number of ticks per second. Avoid loss-of-precision by first converting to
  // milliseconds (i.e., multiplying by 1000) *before* dividing by `f`.
  // Reference:
  // https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
  constexpr auto kOneSecondInMs = 1'000;
  const auto time_in_ms = (number_of_ticks.QuadPart * kOneSecondInMs) /
                          number_of_ticks_per_second.QuadPart;
  return time_in_ms;
}

void utils::win32::LimitFrameRate(INT fps, LONGLONG start_time) {
  // Hint: there are monitors with a refresh rate of `240 / s`.
  constexpr auto kMaximumFps = 240;
  assert(fps > 0 && fps <= kMaximumFps && start_time > 0);
  const auto max_duration = 1'000 / fps;
  const auto end_time = GetMilliseconds();
  const auto duration = end_time - start_time;
  if (duration < max_duration) {
    const auto sleep_duration = static_cast<DWORD>(max_duration - duration);
    Sleep(sleep_duration);
  }
}

HWND utils::win32::CreateMainWindow(HINSTANCE instance, LONG width,
                                    LONG height) {
  // Register window class.
  WNDCLASSEXW window_class = {};
  window_class.cbSize = sizeof(WNDCLASSEX);
  window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  window_class.lpfnWndProc = MainWindowCallback;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = instance;
  window_class.hIcon = LoadIcon(instance, IDI_APPLICATION);
  window_class.hCursor = LoadCursor(instance, IDC_ARROW);
  window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  window_class.lpszMenuName = nullptr;
  window_class.lpszClassName = TEXT("Main Window");
  window_class.hIconSm = LoadIcon(instance, IDI_APPLICATION);
  const bool ok = RegisterClassEx(&window_class) != 0;
  if (!ok) {
    return nullptr;
  }
  // Get the needed window size to fit the client area.
  RECT window_size = {};
  window_size.right = width;
  window_size.bottom = height;
  AdjustWindowRect(&window_size, WS_SYSMENU | WS_CAPTION, FALSE);
  // Create window.
  HWND window =
      CreateWindowEx(WS_OVERLAPPED, window_class.lpszClassName, TEXT("App"),
                     WS_SYSMENU | WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT,
                     window_size.right - window_size.left,
                     window_size.bottom - window_size.top, nullptr, nullptr,
                     instance, nullptr);

  if (window != nullptr) {
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);
  }
  return window;
}
