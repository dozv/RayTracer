#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <Windows.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

#include "common/common.h"
#include "graphics/fps_camera.h"
#include "graphics/mesh_view.h"
#include "graphics/model.h"
#include "graphics/ray_tracer.h"
#include "utils/win32.h"
#include "utils/xm.h"

namespace {
inline void CreateCameraRay(DirectX::XMVECTOR& out_origin,
                            DirectX::XMVECTOR& out_direction, unsigned int x,
                            unsigned int y, int width, int height,
                            const DirectX::XMMATRIX& camera_to_world_matrix) {
  // Calculate half of the horizontal field of view angle (in radians).
  const float fov_horizontal = std::numbers::pi_v<float> / 2.0f;
  const float half_angle_tan = std::tan(fov_horizontal / 2.0f);

  // Calculate the inverse aspect ratio.
  const float aspect_ratio =
      static_cast<float>(width) / static_cast<float>(height);
  const float inverse_aspect_ratio = 1.0f / aspect_ratio;

  const float ndc_x = std::lerp(
      -1.0f, 1.0f, (static_cast<float>(x) + 0.5f) / static_cast<float>(width));
  const float ndc_y = std::lerp(
      1.0f, -1.0f, (static_cast<float>(y) + 0.5f) / static_cast<float>(height));

  const float camera_x = half_angle_tan * ndc_x;
  const float camera_y = inverse_aspect_ratio * half_angle_tan * ndc_y;

  const DirectX::XMVECTOR camera_ndc =
      DirectX::XMVectorSet(camera_x, camera_y, -1.0f, 0.0f);

  const DirectX::XMVECTOR xm_world_origin =
      DirectX::XMVector3Transform(DirectX::g_XMZero, camera_to_world_matrix);
  const DirectX::XMVECTOR xm_world_target =
      DirectX::XMVector3Transform(camera_ndc, camera_to_world_matrix);

  out_origin = xm_world_origin;
  out_direction = DirectX::XMVector3Normalize(
      DirectX::XMVectorSubtract(xm_world_target, xm_world_origin));
}
}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                    _In_ PWSTR cmd_line, _In_ int cmd_show) {
  UNREFERENCED_PARAMETER(cmd_line);
  UNREFERENCED_PARAMETER(cmd_show);
  UNREFERENCED_PARAMETER(prev_instance);
  constexpr auto kWidth = 320;
  constexpr auto kHeight = 240;

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

  std::vector<DirectX::XMUINT2> pixels(kWidth * kHeight);
  for (auto y = 0U; y < kHeight; ++y) {
    for (auto x = 0U; x < kWidth; ++x) {
      pixels[static_cast<size_t>(y) * kWidth + x] = {x, y};
    }
  }

  std::vector<
      std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>>
      meshes{};
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadOctahedron());
  meshes.push_back(model::LoadRectangle());
  meshes.push_back(model::LoadRectangle());

  auto cube_1 = graphics::MeshView(meshes[0].first, meshes[0].second);
  auto cube_2 = graphics::MeshView(meshes[1].first, meshes[1].second);
  auto cube_3 = graphics::MeshView(meshes[2].first, meshes[2].second);
  auto octahedron = graphics::MeshView(meshes[3].first, meshes[3].second);
  auto rectangle_1 = graphics::MeshView(meshes[4].first, meshes[4].second);
  auto rectangle_2 = graphics::MeshView(meshes[5].first, meshes[5].second);

  cube_1.Translate(0.0f, 0.0f, -4.0f);
  cube_2.Translate(0.0f, 2.0f, -8.0f);
  cube_3.Rotate(0.0f, 0.2f, 0.0f)
      .Scale(3.0f, 3.0f, 3.0f)
      .Translate(0.0f, -2.0f, -16.0f);
  octahedron.Rotate(0.2f, 0.2f, 0.1f)
      .Scale(1.0f, 1.0f, 1.0f)
      .Translate(0.0f, 2.0f, -32.0f);
  rectangle_1.Rotate(3.14f / 2.0f, 0.0f, 0.0f)
      .Scale(256.0f, 1.0f, 256.0f)
      .Translate(0.0f, -8.0f, -2.0f);
  rectangle_2.Scale(256.0f, 256.0f, 1.0f).Translate(0.0f, 120.0f, -130.0f);

  constexpr auto kFps = 30;
  bool running = true;
  auto page = 0U;
  constexpr auto kSimulationTimeStep = 1000 / kFps;
  LONGLONG simulation_time = 0;
  std::bitset<256> key_states{};
  std::bitset<256> prev_key_states{};
  auto fps_camera = FpsCamera();
  DirectX::XMVECTOR light_position =
      DirectX::XMVectorSet(0.0f, 0.0f, 0.1f, 0.0f);

  /* std::array<DirectX::XMFLOAT3A, 2> light_positions = {
      DirectX::XMFLOAT3A(0.0f, 0.0f, -1.0f),
      DirectX::XMFLOAT3A(0.0f, 4.0f, -8.0f)};*/

  std::array<DirectX::XMFLOAT3A, 1> light_positions = {
      DirectX::XMFLOAT3A(0.0f, 0.0f, -1.0f)};

  auto shadow_visibility = ray_tracer::ShadowVisibility::Hidden;
  auto reflection_visibility = ray_tracer::ReflectionVisibility::Hidden;

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
    for (auto i = 0U; i < key_states.size(); ++i) {
      key_states[i] = utils::win32::IsKeyPressed(static_cast<INT>(i));
    }
    if (key_states[VK_F1] && !prev_key_states[VK_F1]) {
      shadow_visibility =
          (shadow_visibility == ray_tracer::ShadowVisibility::Visible)
              ? ray_tracer::ShadowVisibility::Hidden
              : ray_tracer::ShadowVisibility::Visible;
    }

    if (key_states[VK_F2] && !prev_key_states[VK_F2]) {
      reflection_visibility =
          (reflection_visibility == ray_tracer::ReflectionVisibility::Visible)
              ? ray_tracer::ReflectionVisibility::Hidden
              : ray_tracer::ReflectionVisibility::Visible;
    }

    // Update previous key states.
    prev_key_states = key_states;

    // Update.
    while (simulation_time < real_time) {
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

    cube_3.Rotate(0.0f, 0.2f, 0.0f);

    // Render.
    page ^= 1;
    auto& current_view = page ? front_buffer : back_buffer;
    auto& display_view = page ? back_buffer : front_buffer;

    std::span<DirectX::XMUINT2> pixel_span(pixels);

    std::for_each(
        std::execution::par, pixels.begin(), pixels.end(), [&](auto& p) {
          const auto x = p.x;
          const auto y = p.y;

          DirectX::XMVECTOR origin = {};
          DirectX::XMVECTOR direction = {};
          CreateCameraRay(origin, direction, x, y, kWidth, kHeight,
                          camera_to_world_matrix);

          auto color =
              ray_tracer::TraceRays(shadow_visibility, reflection_visibility,
                                    meshes, direction, origin, light_positions);

          color = DirectX::XMVectorMultiply(color,
                                            DirectX::XMVectorReplicate(255.0f));

          current_view.At(y, x) = utils::win32::CreateHighColor(
              static_cast<uint8_t>(DirectX::XMVectorGetX(color)),
              static_cast<uint8_t>(DirectX::XMVectorGetY(color)),
              static_cast<uint8_t>(DirectX::XMVectorGetZ(color)));
        });

    // Render to window.
    HDC device_context = GetDC(window);
    StretchDIBits(device_context, 0, 0, kDoubleWidth, kDoubleHeight, 0, 0,
                  static_cast<INT>(display_view.Columns()),
                  static_cast<INT>(display_view.Rows()),
                  display_view.Elements().data(), &bmi, DIB_RGB_COLORS,
                  SRCCOPY);
    ReleaseDC(window, device_context);

    utils::win32::LimitFrameRate(kFps, real_time);
  }

  return 0;
}
