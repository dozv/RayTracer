#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <Windows.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <vector>

#include "common/common.h"
#include "fps_camera.h"
#include "utils/win32.h"
#include "utils/xm.h"

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                    _In_ PWSTR cmd_line, _In_ int cmd_show) {
  constexpr auto kWidth = 320;
  constexpr auto kHeight = 240;
  constexpr auto kAspectRatio =
      static_cast<float>(kWidth) / static_cast<float>(kHeight);
  constexpr auto kInverseAspectRatio = 1.0f / kAspectRatio;
  constexpr auto kFov = DirectX::XMConvertToRadians(90.0f);
  const auto kHalfAngleTan = tan(kFov / 2.0f);
  constexpr auto kDoubleWidth = kWidth * 2;
  constexpr auto kDoubleHeight = kHeight * 2;
  HWND window =
      utils::win32::CreateMainWindow(instance, kDoubleWidth, kDoubleHeight);

  std::vector<UINT16> frame_buffer(kDoubleWidth * kDoubleHeight, UINT16{});
  auto frame_buffer_view =
      MatrixView<UINT16>(frame_buffer, kDoubleWidth, kDoubleHeight);
  auto front_buffer = frame_buffer_view.Submatrix(0, 0, kHeight, kWidth);
  auto back_buffer = frame_buffer_view.Submatrix(kHeight, 0, kHeight, kWidth);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kWidth;
  bmi.bmiHeader.biHeight = -kHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 16;
  bmi.bmiHeader.biClrUsed = BI_RGB;

  std::vector<DirectX::XMFLOAT3A> vertices(
      {DirectX::XMFLOAT3A(1.0f, -1.0f, -4.0f),
       DirectX::XMFLOAT3A(-1.0f, -1.0f, -4.0f),
       DirectX::XMFLOAT3A(0.0f, 1.0f, -4.0f),
       DirectX::XMFLOAT3A(1.5f, -1.5f, -5.0f),
       DirectX::XMFLOAT3A(-0.5f, -1.5f, -5.0f),
       DirectX::XMFLOAT3A(0.5f, 0.5f, -5.0f)});

  std::vector<DirectX::XMFLOAT3A> vertex_colors(
      {DirectX::XMFLOAT3A(1.0f, 0.0f, 0.0f),
       DirectX::XMFLOAT3A(0.0f, 1.0f, 0.0f),
       DirectX::XMFLOAT3A(0.0f, 0.0f, 1.0f),
       DirectX::XMFLOAT3A(1.0f, 1.0f, 0.0f),
       DirectX::XMFLOAT3A(0.0f, 1.0f, 1.0f),
       DirectX::XMFLOAT3A(1.0f, 0.0f, 1.0f)});

  std::vector<DirectX::XMINT3> faces(
      {DirectX::XMINT3(0, 1, 2), DirectX::XMINT3(3, 4, 5)});

  constexpr auto kFps = 30;
  LONGLONG last_time = utils::win32::GetMilliseconds();
  bool running = true;
  auto page = 0U;
  constexpr auto kSimulationTimeStep = 1000 / kFps;
  LONGLONG simulation_time = 0;
  std::bitset<256> key_states{};
  auto fps_camera = FpsCamera();

  while (running) {
    const auto real_time = utils::win32::GetMilliseconds();

    // Check for window messages.
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      if (msg.message == WM_KEYDOWN) {
        if (msg.wParam == VK_ESCAPE) {
          Beep(1000, 100);
          if (MessageBox(window, TEXT("Really quit?"), TEXT("Quit"),
                         MB_OKCANCEL) == IDOK) {
            running = false;
          }
        }
      } else if (msg.message == WM_QUIT) {
        running = false;
      }
    }

    // Get keyboard input.
    for (auto i = 0; i < key_states.size(); ++i) {
      key_states[i] = utils::win32::IsKeyPressed(i);
    }

    // Update.
    while (simulation_time < real_time) {
      // TODO(Daniel): physics updates.
      constexpr auto kSpeed = 1E-2f;
      constexpr auto kRotationSpeed = 1E-1f;

      float delta_move = kSpeed * kSimulationTimeStep;
      float delta_rotate = kRotationSpeed * kSimulationTimeStep;

      if (key_states['W']) {
        fps_camera.Move(delta_move, 0.0F);  // Move forward.
      } else if (key_states['S']) {
        fps_camera.Move(-delta_move, 0.0F);  // Move backward.
      } else if (key_states['A']) {
        fps_camera.Move(0.0F, delta_move);  // Move left.
      } else if (key_states['D']) {
        fps_camera.Move(0.0F, -delta_move);  // Move right.
      } else if (key_states[VK_DOWN]) {
        fps_camera.Rotate(-delta_rotate, 0.0F);  // Look down.
      } else if (key_states[VK_UP]) {
        fps_camera.Rotate(delta_rotate, 0.0F);  // Look up.
      } else if (key_states[VK_LEFT]) {
        fps_camera.Rotate(0.0F, delta_rotate);  // Look left.
      } else if (key_states[VK_RIGHT]) {
        fps_camera.Rotate(0.0F, -delta_rotate);  // Look right.
      }

      simulation_time += kSimulationTimeStep;
    }

    auto camera_to_world_matrix = fps_camera.GetCameraToWorldMatrix();

    // Render.
    auto& current_view = (page ^= 1) ? front_buffer : back_buffer;

    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kWidth; ++x) {
        const float ndc_x = RemapRange(static_cast<float>(x) + 0.5f, 0.0f,
                                       static_cast<float>(kWidth), -1.0f, 1.0f);
        const float ndc_y =
            RemapRange(static_cast<float>(y) + 0.5f,
                       static_cast<float>(kHeight), 0.0f, -1.0f, 1.0f);

        const float camera_x = kAspectRatio * kHalfAngleTan * ndc_x;
        const float camera_y = kInverseAspectRatio * kHalfAngleTan * ndc_y;

        DirectX::XMVECTOR xm_camera_origin = DirectX::g_XMZero;
        DirectX::XMVECTOR xm_camera_target = DirectX::g_XMZero;
        DirectX::XMVECTOR xm_world_origin = DirectX::XMVector3Transform(
            xm_camera_origin, camera_to_world_matrix);
        DirectX::XMVECTOR xm_world_target = DirectX::XMVector3Transform(
            DirectX::XMVectorSet(camera_x, camera_y, -1.0f, 0.0f),
            camera_to_world_matrix);
        auto xm_world_direction = DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(xm_world_target, xm_world_origin));

        float closest_distance = std::numeric_limits<float>::infinity();
        DirectX::XMFLOAT3A color = {0.0f, 0.0f, 0.0f};

        for (auto& face : faces) {
          DirectX::XMVECTOR xm_a, xm_b, xm_c;
          utils::xm::triangle::Load(xm_a, xm_b, xm_c, vertices, face);

          float distance = 0.0f;
          if (DirectX::TriangleTests::Intersects(xm_world_origin,
                                                 xm_world_direction, xm_a, xm_b,
                                                 xm_c, distance) &&
              distance > 1.0f && distance < closest_distance) {
            const auto intersection_point = utils::xm::ray::At(
                xm_world_origin, xm_world_direction, distance);
            const auto barycentrics = utils::xm::triangle::GetBarycentrics(
                xm_a, xm_b, xm_c, intersection_point);

            if (!utils::xm::triangle::IsPointInside(barycentrics)) break;

            utils::xm::triangle::Load(xm_a, xm_b, xm_c, vertex_colors, face);
            const auto xm_color = utils::xm::triangle::Interpolate(
                xm_a, xm_b, xm_c, barycentrics);
            color = utils::xm::float3a::Store(xm_color);

            closest_distance = distance;
          }
        }

        current_view.At(y, x) =
            utils::win32::CreateHighColor(static_cast<uint8_t>(color.x * 255),
                                          static_cast<uint8_t>(color.y * 255),
                                          static_cast<uint8_t>(color.z * 255));
      }
    }

    HDC device_context = GetDC(window);
    StretchDIBits(device_context, 0, 0, kDoubleWidth, kDoubleHeight, 0, 0,
                  current_view.Columns(), current_view.Rows(),
                  current_view.Elements().data(), &bmi, DIB_RGB_COLORS,
                  SRCCOPY);
    ReleaseDC(window, device_context);

    utils::win32::LimitFrameRate(kFps, real_time);
  }

  return 0;
}
