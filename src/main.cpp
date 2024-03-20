#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <Windows.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <optional>
#include <vector>

#include "common/common.h"
#include "fps_camera.h"
#include "model.h"
#include "utils/win32.h"
#include "utils/xm.h"

namespace {
inline void TraceShadowRays(std::span<model::Mesh> meshes,
                            DirectX::FXMVECTOR world_intersection,
                            DirectX::FXMVECTOR world_shadow_direction,
                            DirectX::FXMVECTOR light_position,
                            DirectX::XMFLOAT3A& color, bool& in_shadow,
                            unsigned int mesh_index, unsigned int face_index) {
  for (auto shadow_mesh_index = 0U;
       !in_shadow && shadow_mesh_index < meshes.size(); ++shadow_mesh_index) {
    for (auto shadow_face_index = 0U;
         !in_shadow &&
         shadow_face_index < meshes[shadow_mesh_index].second.size();
         ++shadow_face_index) {
      // Skip the current face or mesh.
      if (shadow_mesh_index == mesh_index || shadow_face_index == face_index) {
        continue;
      }

      DirectX::XMVECTOR shadow_a{};
      DirectX::XMVECTOR shadow_b{};
      DirectX::XMVECTOR shadow_c{};
      utils::xm::triangle::Load(
          shadow_a, shadow_b, shadow_c, meshes[shadow_mesh_index].first,
          meshes[shadow_mesh_index].second[shadow_face_index]);

      auto offset_world_origin = DirectX::XMVectorAdd(
          world_intersection, DirectX::XMVectorReplicate(1E-4f));

      auto shadow_result = utils::xm::triangle::Intersect(
          shadow_a, shadow_b, shadow_c, offset_world_origin,
          world_shadow_direction);

      if (shadow_result.has_value() && shadow_result->z > 0.0f &&
          shadow_result->z < DirectX::XMVectorGetX(DirectX::XMVector3LengthEst(
                                 DirectX::XMVectorSubtract(
                                     light_position, world_intersection)))) {
        in_shadow = true;
        // Darken the color.
        color = utils::xm::float3a::Clamp(color, 0.00390625f, 0.0625f);
      }
    }  // for-each shadow triangle.
  }    // for-each shadow mesh.
}

inline void TraceReflectionRays(std::span<model::Mesh> meshes,
                                DirectX::XMVECTOR world_intersection,
                                DirectX::XMVECTOR xm_world_direction,
                                DirectX::XMVECTOR xm_surface_normal,
                                DirectX::XMFLOAT3A& color,
                                unsigned int mesh_index,
                                unsigned int face_index) {
  for (auto reflection_mesh_index = 0U; reflection_mesh_index < meshes.size();
       ++reflection_mesh_index) {
    for (auto reflection_face_index = 0U;
         reflection_face_index < meshes[reflection_mesh_index].second.size();
         ++reflection_face_index) {
      // Skip the current face or mesh.
      if (reflection_mesh_index == mesh_index &&
          reflection_face_index == face_index) {
        continue;
      }
      DirectX::XMVECTOR reflection_a{};
      DirectX::XMVECTOR reflection_b{};
      DirectX::XMVECTOR reflection_c{};
      utils::xm::triangle::Load(
          reflection_a, reflection_b, reflection_c,
          meshes[reflection_mesh_index].first,
          meshes[reflection_mesh_index].second[reflection_face_index]);
      const auto reflection_direction = DirectX::XMVector3NormalizeEst(
          DirectX::XMVector3Reflect(xm_world_direction, xm_surface_normal));

      const auto offset = 1E-2f;
      const auto xm_world_origin_offset = DirectX::XMVectorAdd(
          world_intersection,
          DirectX::XMVectorScale(reflection_direction, offset));

      auto reflection_result = utils::xm::triangle::Intersect(
          reflection_a, reflection_b, reflection_c, xm_world_origin_offset,
          reflection_direction);

      if (reflection_result.has_value() && reflection_result->z > 0.0f) {
        // Calculate the color at the reflection intersection
        // point.
        DirectX::XMVECTOR reflection_color = DirectX::XMVectorSet(
            (1.0f - reflection_result->x - reflection_result->y) * color.x,
            reflection_result->x * color.y, reflection_result->y * color.z,
            0.0f);

        constexpr float reflectivity = 0.95f;
        color = utils::xm::float3a::Store(DirectX::XMVectorSaturate(
            DirectX::XMVectorLerp(utils::xm::float3a::Load(color),
                                  reflection_color, reflectivity)));
      }
    }
  }
}

inline DirectX::XMFLOAT3A TraceRays(std::span<model::Mesh> meshes,
                                    DirectX::XMVECTOR xm_world_direction,
                                    DirectX::XMVECTOR xm_world_origin,
                                    DirectX::XMVECTOR light_position,
                                    bool& visible_shadows,
                                    bool& show_reflections) {
  // Trace camera rays.
  float closest_distance = std::numeric_limits<float>::infinity();
  const auto ambient = DirectX::XMVectorReplicate(0.2f);
  DirectX::XMFLOAT3A color = {1.0f, 1.0f, 1.0f};

  for (auto mesh_index = 0U; mesh_index < meshes.size(); ++mesh_index) {
    for (auto face_index = 0U; face_index < meshes[mesh_index].second.size();
         ++face_index) {
      DirectX::XMVECTOR xm_a{};
      DirectX::XMVECTOR xm_b{};
      DirectX::XMVECTOR xm_c{};
      utils::xm::triangle::Load(xm_a, xm_b, xm_c, meshes[mesh_index].first,
                                meshes[mesh_index].second[face_index]);

      auto result = utils::xm::triangle::Intersect(
          xm_a, xm_b, xm_c, xm_world_origin, xm_world_direction);

      if (result.has_value() && result->z > 1.0f &&
          result->z < closest_distance) {
        const auto world_intersection =
            utils::xm::ray::At(xm_world_origin, xm_world_direction, result->z);

        DirectX::XMVECTOR world_shadow_direction =
            utils::xm::ray::GetNormalizedDirectionFromPoints(world_intersection,
                                                             light_position);

        DirectX::XMVECTOR barycentrics = DirectX::XMVectorSet(
            1.0f - result->x - result->y, result->x, result->y, 1.0f);

        // Trace shadow rays.
        bool in_shadow = false;
        if (visible_shadows) {
          TraceShadowRays(meshes, world_intersection, world_shadow_direction,
                          light_position, color, in_shadow, mesh_index,
                          face_index);
        }

        // Shade using the Lambertian illumination model, if there is no
        // shadow.
        DirectX::XMVECTOR xm_surface_normal = DirectX::XMVector3Normalize(
            utils::xm::triangle::GetSurfaceNormal(xm_a, xm_b, xm_c));
        if (!in_shadow) {
          DirectX::XMVECTOR xm_light_direction = DirectX::XMVector3Normalize(
              DirectX::XMVectorSubtract(light_position, world_intersection));

          float lambertian = DirectX::XMVectorGetX(
              DirectX::XMVector3Dot(xm_surface_normal, xm_light_direction));

          DirectX::XMVECTOR xm_lambertian =
              DirectX::XMVectorReplicate(lambertian * 0.8f);

          color = utils::xm::float3a::Store(
              DirectX::XMVectorSaturate(DirectX::XMVectorMultiplyAdd(
                  barycentrics, xm_lambertian, ambient)));
        }  // if (not in shadow).

        // Trace reflection rays.
        if (show_reflections) {
          TraceReflectionRays(meshes, world_intersection, xm_world_direction,
                              xm_surface_normal, color, mesh_index, face_index);
        }  // if (show reflections).

        closest_distance = result->z;
      }  // if (intersection).
    }    // for-each face (triangle).
  }      // for-each mesh.

  return color;
}
}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                    _In_ PWSTR cmd_line, _In_ int cmd_show) {
  UNREFERENCED_PARAMETER(cmd_line);
  UNREFERENCED_PARAMETER(cmd_show);
  UNREFERENCED_PARAMETER(prev_instance);
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

  std::vector<
      std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>>
      meshes{};
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadCube());
  meshes.push_back(model::LoadOctahedron());
  meshes.push_back(model::LoadRectangle());
  meshes.push_back(model::LoadRectangle());

  std::vector<DirectX::XMUINT2> pixels(kWidth * kHeight);
  for (auto y = 0U; y < kHeight; ++y) {
    for (auto x = 0U; x < kWidth; ++x) {
      pixels[y * kWidth + x] = {x, y};
    }
  }

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex, DirectX::XMMatrixTranslation(0.0f, 0.0f, -4.0f));
      },
      meshes[0].first, meshes[0].first);

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex, DirectX::XMMatrixTranslation(0.0f, 2.0f, -8.0f));
      },
      meshes[1].first, meshes[1].first);

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex, DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.2f, 0.0f) *
                        DirectX::XMMatrixScaling(3.0f, 3.0f, 3.0f) *
                        DirectX::XMMatrixTranslation(0.0f, -2.0f, -16.0f));
      },
      meshes[2].first, meshes[2].first);

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex, DirectX::XMMatrixRotationRollPitchYaw(0.2f, 0.2f, 0.1f) *
                        DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) *
                        DirectX::XMMatrixTranslation(0.0f, 2.0f, -32.0f));
      },
      meshes[3].first, meshes[3].first);

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex,
            DirectX::XMMatrixRotationRollPitchYaw(3.14 / 2.0f, 0.0f, 0.0f) *
                DirectX::XMMatrixScaling(256.0f, 1.0f, 256.0f) *
                DirectX::XMMatrixTranslation(0.0f, -8.0f, -2.0f));
      },
      meshes[4].first, meshes[4].first);

  utils::xm::ApplyTransform(
      [&](auto vertex) {
        return utils::xm::float3a::Transform(
            vertex, DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) *
                        DirectX::XMMatrixScaling(256.0f, 256.0f, 1.0f) *
                        DirectX::XMMatrixTranslation(0.0f, 120.0f, -130.0f));
      },
      meshes[5].first, meshes[5].first);

  constexpr auto kFps = 30;
  bool running = true;
  auto page = 0U;
  constexpr auto kSimulationTimeStep = 1000 / kFps;
  LONGLONG simulation_time = 0;
  std::bitset<256> key_states{};
  auto fps_camera = FpsCamera();
  DirectX::XMVECTOR light_position =
      DirectX::XMVectorSet(0.0f, 10.0f, 8.0f, 0.0f);

  bool visible_shadows = false;
  bool show_reflections = false;

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

    if (key_states[VK_F1]) {
      visible_shadows = !visible_shadows;
    }

    if (key_states[VK_F2]) {
      show_reflections = !show_reflections;
    }

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

    utils::xm::ApplyTransform(
        [&](auto vertex) {
          return utils::xm::float3a::Transform(
              vertex, DirectX::XMMatrixRotationRollPitchYaw(0.0f, 0.2f, 0.0f));
        },
        meshes[2].first, meshes[2].first);

    // Render.
    auto& current_view = (page ^= 1) ? front_buffer : back_buffer;

    std::span<DirectX::XMUINT2> pixel_span(pixels);

    std::for_each(
        std::execution::par, pixels.begin(), pixels.end(), [&](auto& p) {
          const auto x = p.x;
          const auto y = p.y;

          const float ndc_x =
              RemapRange(static_cast<float>(x) + 0.5f, 0.0f,
                         static_cast<float>(kWidth), -1.0f, 1.0f);
          const float ndc_y =
              RemapRange(static_cast<float>(y) + 0.5f,
                         static_cast<float>(kHeight), 0.0f, -1.0f, 1.0f);

          const float camera_x = kHalfAngleTan * ndc_x;
          const float camera_y = kInverseAspectRatio * kHalfAngleTan * ndc_y;

          DirectX::XMVECTOR xm_camera_origin = DirectX::g_XMZero;
          DirectX::XMVECTOR xm_camera_target = DirectX::g_XMZero;
          DirectX::XMVECTOR xm_world_origin = DirectX::XMVector3Transform(
              xm_camera_origin, camera_to_world_matrix);
          DirectX::XMVECTOR xm_world_target = DirectX::XMVector3Transform(
              DirectX::XMVectorSet(camera_x, camera_y, -1.0f, 0.0f),
              camera_to_world_matrix);
          const auto xm_world_direction =
              utils::xm::ray::GetNormalizedDirectionFromPoints(xm_world_origin,
                                                               xm_world_target);

          const auto color =
              TraceRays(meshes, xm_world_direction, xm_world_origin,
                        light_position, visible_shadows, show_reflections);

          current_view.At(y, x) = utils::win32::CreateHighColor(
              static_cast<uint8_t>(color.x * 255),
              static_cast<uint8_t>(color.y * 255),
              static_cast<uint8_t>(color.z * 255));
        });

    HDC device_context = GetDC(window);
    StretchDIBits(device_context, 0, 0, kDoubleWidth, kDoubleHeight, 0, 0,
                  static_cast<INT>(current_view.Columns()),
                  static_cast<INT>(current_view.Rows()),
                  current_view.Elements().data(), &bmi, DIB_RGB_COLORS,
                  SRCCOPY);
    ReleaseDC(window, device_context);

    utils::win32::LimitFrameRate(kFps, real_time);
  }

  return 0;
}
