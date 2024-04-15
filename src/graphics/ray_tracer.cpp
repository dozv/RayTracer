#include "ray_tracer.h"

#include <algorithm>
#include <ranges>

namespace {
inline float CalculateDistance(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b) {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3LengthEst(DirectX::XMVectorSubtract(b, a)));
}

struct MeshBatch {
  std::span<model::Mesh> meshes;
  size_t current_mesh_index;
  size_t current_face_index;
};

inline bool IsShadowed(DirectX::XMVECTOR& color, const MeshBatch& batch,
                       DirectX::FXMVECTOR world_intersection,
                       const DirectX::XMFLOAT3A& light_position) {
  constexpr float kShadowIntensity = 0.0625f;
  float total_intensity = 0.0f;

  auto world_shadow_direction = utils::xm::ray::CalculateDirection(
      world_intersection, utils::xm::float3a::Load(light_position));

  for (size_t mesh_index = 0; mesh_index < batch.meshes.size(); ++mesh_index) {
    if (mesh_index == batch.current_mesh_index) continue;

    const auto& mesh = batch.meshes[mesh_index];
    for (size_t face_index = 0; face_index < mesh.second.size(); ++face_index) {
      if (mesh_index == batch.current_mesh_index &&
          face_index == batch.current_face_index)
        continue;

      const auto& face = mesh.second[face_index];
      DirectX::XMVECTOR shadow_a{}, shadow_b{}, shadow_c{};
      utils::xm::triangle::Load(shadow_a, shadow_b, shadow_c, mesh.first, face);
      auto offset_world_origin =
          DirectX::XMVectorAdd(world_intersection, DirectX::g_XMEpsilon);
      auto shadow_result = utils::xm::triangle::Intersect(
          shadow_a, shadow_b, shadow_c, offset_world_origin,
          world_shadow_direction);

      if (shadow_result.has_value() && shadow_result->z > 0.0f &&
          shadow_result->z <
              CalculateDistance(DirectX::XMLoadFloat3A(&light_position),
                                world_intersection)) {
        total_intensity += kShadowIntensity;
        // No need to check further, already in shadow.
        break;
      }
    }
  }

  // Update the color based on the total intensity.
  color = DirectX::XMVectorReplicate(total_intensity);

  return total_intensity > 0.0f;
}

inline void TraceReflectionRay(DirectX::XMVECTOR& color,
                               const MeshBatch& params,
                               DirectX::FXMVECTOR world_intersection,
                               DirectX::FXMVECTOR xm_world_direction,
                               DirectX::FXMVECTOR xm_surface_normal) {
  for (auto reflection_mesh_index = 0U;
       reflection_mesh_index < params.meshes.size(); ++reflection_mesh_index) {
    // Skip the current mesh.
    if (reflection_mesh_index == params.current_mesh_index) {
      continue;
    }

    for (auto reflection_face_index = 0U;
         reflection_face_index <
         params.meshes[reflection_mesh_index].second.size();
         ++reflection_face_index) {
      DirectX::XMVECTOR reflection_a{};
      DirectX::XMVECTOR reflection_b{};
      DirectX::XMVECTOR reflection_c{};
      utils::xm::triangle::Load(
          reflection_a, reflection_b, reflection_c,
          params.meshes[reflection_mesh_index].first,
          params.meshes[reflection_mesh_index].second[reflection_face_index]);
      const auto reflection_direction = DirectX::XMVector3NormalizeEst(
          DirectX::XMVector3Reflect(xm_world_direction, xm_surface_normal));

      const auto offset_world_origin =
          DirectX::XMVectorAdd(world_intersection, DirectX::g_XMEpsilon);

      auto reflection_result = utils::xm::triangle::Intersect(
          reflection_a, reflection_b, reflection_c, offset_world_origin,
          reflection_direction);

      if (reflection_result.has_value() && reflection_result->z > 0.0f) {
        // Calculate the color at the reflection intersection
        // point.
        const auto bc = DirectX::XMVectorSet(
            1.0f - reflection_result->x - reflection_result->y,
            reflection_result->x, reflection_result->y, 0.0f);

        const auto reflection_color = DirectX::XMVectorMultiply(bc, color);

        constexpr float reflectivity = 0.95f;
        color = DirectX::XMVectorSaturate(
            DirectX::XMVectorLerp(color, reflection_color, reflectivity));
      }
    }
  }
}
}  // namespace

DirectX::XMVECTOR ray_tracer::TraceRays(
    bool& visible_shadows, bool& show_reflections,
    std::span<model::Mesh> meshes, DirectX::FXMVECTOR xm_world_direction,
    DirectX::FXMVECTOR xm_world_origin,
    std::span<const DirectX::XMFLOAT3A> light_positions) {
  float closest_distance = std::numeric_limits<float>::infinity();
  const auto ambient = DirectX::XMVectorReplicate(0.2f);
  DirectX::XMVECTOR color = DirectX::g_XMOne;

  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    for (size_t face_index = 0; face_index < meshes[mesh_index].second.size();
         ++face_index) {
      DirectX::XMVECTOR xm_a{};
      DirectX::XMVECTOR xm_b{};
      DirectX::XMVECTOR xm_c{};
      utils::xm::triangle::Load(xm_a, xm_b, xm_c, meshes[mesh_index].first,
                                meshes[mesh_index].second[face_index]);

      const auto result = utils::xm::triangle::Intersect(
          xm_a, xm_b, xm_c, xm_world_origin, xm_world_direction);

      if (result.has_value() && result->z > 1.0f &&
          result->z < closest_distance) {
        const auto world_intersection =
            utils::xm::ray::At(xm_world_origin, xm_world_direction, result->z);

        DirectX::XMVECTOR barycentrics = DirectX::XMVectorSet(
            1.0f - result->x - result->y, result->x, result->y, 1.0f);

        DirectX::XMVECTOR xm_surface_normal = DirectX::XMVector3Normalize(
            utils::xm::triangle::GetSurfaceNormal(xm_a, xm_b, xm_c));

        DirectX::XMVECTOR accumulated_color = DirectX::g_XMZero;

        for (const auto& light_position : light_positions) {
          bool in_shadow = false;
          if (visible_shadows) {
            DirectX::XMVECTOR shadow_color = color;
            in_shadow |=
                IsShadowed(shadow_color, {meshes, mesh_index, face_index},
                           world_intersection, light_position);
            color = shadow_color;
          }

          if (!in_shadow) {
            DirectX::XMVECTOR xm_light_direction =
                utils::xm::ray::CalculateDirection(
                    world_intersection,
                    utils::xm::float3a::Load(light_position));

            float lambertian = DirectX::XMVectorGetX(
                DirectX::XMVector3Dot(xm_surface_normal, xm_light_direction));

            DirectX::XMVECTOR xm_lambertian =
                DirectX::XMVectorReplicate(lambertian * 0.8f);

            accumulated_color = DirectX::XMVectorMultiplyAdd(
                barycentrics, xm_lambertian, accumulated_color);
          }
        }

        accumulated_color = DirectX::XMVectorMultiplyAdd(
            ambient, DirectX::g_XMOne, accumulated_color);
        color = DirectX::XMVectorSaturate(accumulated_color);

        if (show_reflections) {
          TraceReflectionRay(color, {meshes, mesh_index, face_index},
                             world_intersection, xm_world_direction,
                             xm_surface_normal);
        }

        closest_distance = result->z;
      }
    }
  }

  return color;
}
