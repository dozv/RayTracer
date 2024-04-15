#include "ray_tracer.h"

#include <algorithm>
#include <ranges>

namespace {
// Calculate distance between two vectors.
inline float CalculateDistance(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b) {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3LengthEst(DirectX::XMVectorSubtract(b, a)));
}

// Check if the ray from intersection point to light intersects with any mesh.
inline bool IntersectsWithShadow(const std::span<model::Mesh>& meshes,
                                 size_t mesh_index, size_t face_index,
                                 DirectX::FXMVECTOR intersection_point,
                                 const DirectX::XMFLOAT3A& light_position) {
  const auto& face = meshes[mesh_index].second[face_index];
  DirectX::XMVECTOR vertex_a{};
  DirectX::XMVECTOR vertex_b{};
  DirectX::XMVECTOR vertex_c{};
  utils::xm::triangle::Load(vertex_a, vertex_b, vertex_c,
                            meshes[mesh_index].first, face);
  auto offset_intersection_point =
      DirectX::XMVectorAdd(intersection_point, DirectX::g_XMEpsilon);
  auto shadow_direction = utils::xm::ray::CalculateDirection(
      intersection_point, utils::xm::float3a::Load(light_position));
  auto shadow_result = utils::xm::triangle::Intersect(
      vertex_a, vertex_b, vertex_c, offset_intersection_point,
      shadow_direction);

  return shadow_result.has_value() && shadow_result->z > 0.0f &&
         shadow_result->z <
             CalculateDistance(DirectX::XMLoadFloat3A(&light_position),
                               intersection_point);
}

// Check if a point is shadowed by geometry in the scene.
inline bool IsShadowed(DirectX::XMVECTOR& final_color,
                       const std::span<model::Mesh>& meshes,
                       size_t outer_mesh_index, size_t outer_face_index,
                       DirectX::FXMVECTOR intersection_point,
                       const DirectX::XMFLOAT3A& light_position) {
  constexpr float kShadowIntensity = 0.0625f;
  float total_intensity = 0.0f;

  auto shadow_direction = utils::xm::ray::CalculateDirection(
      intersection_point, utils::xm::float3a::Load(light_position));

  // Iterate through meshes and faces.
  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    if (mesh_index == outer_mesh_index) continue;

    const auto& mesh = meshes[mesh_index];
    for (size_t face_index = 0; face_index < mesh.second.size(); ++face_index) {
      if (mesh_index == outer_mesh_index && face_index == outer_face_index)
        continue;

      // Check for intersection.
      if (IntersectsWithShadow(meshes, mesh_index, face_index,
                               intersection_point, light_position)) {
        total_intensity += kShadowIntensity;
        // No need to check further, already in shadow.
        break;
      }
    }
  }

  // Update the color based on the total intensity.
  final_color = DirectX::XMVectorReplicate(total_intensity);

  return total_intensity > 0.0f;
}

// Check if the ray from intersection point to reflection intersects with any
// mesh.
inline bool IntersectsWithReflection(DirectX::XMVECTOR& final_color,
                                     const std::span<model::Mesh>& meshes,
                                     size_t mesh_index, size_t face_index,
                                     DirectX::FXMVECTOR intersection_point,
                                     DirectX::FXMVECTOR incident_direction,
                                     DirectX::FXMVECTOR surface_normal,
                                     float reflectivity) {
  DirectX::XMVECTOR vertex_a{};
  DirectX::XMVECTOR vertex_b{};
  DirectX::XMVECTOR vertex_c{};
  utils::xm::triangle::Load(vertex_a, vertex_b, vertex_c,
                            meshes[mesh_index].first,
                            meshes[mesh_index].second[face_index]);

  // Calculate reflection direction
  const auto reflection_direction = DirectX::XMVector3NormalizeEst(
      DirectX::XMVector3Reflect(incident_direction, surface_normal));

  const auto offset_intersection_point =
      DirectX::XMVectorAdd(intersection_point, DirectX::g_XMEpsilon);

  auto reflection_result = utils::xm::triangle::Intersect(
      vertex_a, vertex_b, vertex_c, offset_intersection_point,
      reflection_direction);

  if (reflection_result.has_value() && reflection_result->z > 0.0f) {
    // Calculate the color at the reflection intersection point.
    const auto barycentric_coords =
        DirectX::XMVectorSet(1.0f - reflection_result->x - reflection_result->y,
                             reflection_result->x, reflection_result->y, 0.0f);

    const auto reflection_color =
        DirectX::XMVectorMultiply(barycentric_coords, final_color);

    final_color = DirectX::XMVectorSaturate(
        DirectX::XMVectorLerp(final_color, reflection_color, reflectivity));
    return true;
  }

  return false;
}

// Trace the reflection ray and calculate reflection color.
inline void TraceReflectionRay(DirectX::XMVECTOR& final_color,
                               const std::span<model::Mesh>& meshes,
                               size_t outer_mesh_index, size_t outer_face_index,
                               DirectX::FXMVECTOR intersection_point,
                               DirectX::FXMVECTOR incident_direction,
                               DirectX::FXMVECTOR surface_normal) {
  constexpr auto kReflectivity = 0.95f;

  const auto reflection_direction = DirectX::XMVector3NormalizeEst(
      DirectX::XMVector3Reflect(incident_direction, surface_normal));

  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    if (mesh_index == outer_mesh_index) {
      continue;  // Skip the same mesh.
    }

    for (size_t face_index = 0; face_index < meshes[mesh_index].second.size();
         ++face_index) {
      if (mesh_index == outer_mesh_index && face_index == outer_face_index) {
        continue;  // Skip the same face.
      }

      // Check for reflection intersection.
      if (IntersectsWithReflection(final_color, meshes, mesh_index, face_index,
                                   intersection_point, incident_direction,
                                   surface_normal, kReflectivity)) {
        break;
      }
    }
  }
}
}  // namespace

DirectX::XMVECTOR ray_tracer::TraceRays(
    ShadowVisibility shadow_visibility,
    ReflectionVisibility reflection_visibility, std::span<model::Mesh> meshes,
    DirectX::FXMVECTOR world_direction, DirectX::FXMVECTOR world_origin,
    std::span<const DirectX::XMFLOAT3A> light_positions) {
  float closest_distance = std::numeric_limits<float>::infinity();
  const auto ambient_color = DirectX::XMVectorReplicate(0.2f);
  DirectX::XMVECTOR result_color = DirectX::g_XMOne;

  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    for (size_t face_index = 0; face_index < meshes[mesh_index].second.size();
         ++face_index) {
      DirectX::XMVECTOR vertex_a{};
      DirectX::XMVECTOR vertex_b{};
      DirectX::XMVECTOR vertex_c{};
      utils::xm::triangle::Load(vertex_a, vertex_b, vertex_c,
                                meshes[mesh_index].first,
                                meshes[mesh_index].second[face_index]);

      const auto intersection_result = utils::xm::triangle::Intersect(
          vertex_a, vertex_b, vertex_c, world_origin, world_direction);

      if (intersection_result.has_value() && intersection_result->z > 1.0f &&
          intersection_result->z < closest_distance) {
        const auto intersection_point = utils::xm::ray::At(
            world_origin, world_direction, intersection_result->z);

        DirectX::XMVECTOR barycentric_coords = DirectX::XMVectorSet(
            1.0f - intersection_result->x - intersection_result->y,
            intersection_result->x, intersection_result->y, 1.0f);

        DirectX::XMVECTOR surface_normal =
            DirectX::XMVector3Normalize(utils::xm::triangle::GetSurfaceNormal(
                vertex_a, vertex_b, vertex_c));

        DirectX::XMVECTOR accumulated_color = DirectX::g_XMZero;

        for (const auto& light_position : light_positions) {
          bool in_shadow = false;
          if (shadow_visibility == ShadowVisibility::Visible) {
            DirectX::XMVECTOR shadow_color = result_color;
            in_shadow |=
                IsShadowed(shadow_color, meshes, mesh_index, face_index,
                           intersection_point, light_position);
            result_color = shadow_color;
          }

          if (!in_shadow) {
            DirectX::XMVECTOR light_direction =
                utils::xm::ray::CalculateDirection(
                    intersection_point,
                    utils::xm::float3a::Load(light_position));

            float lambertian = DirectX::XMVectorGetX(
                DirectX::XMVector3Dot(surface_normal, light_direction));

            DirectX::XMVECTOR lambertian_color =
                DirectX::XMVectorReplicate(lambertian * 0.8f);

            accumulated_color = DirectX::XMVectorMultiplyAdd(
                barycentric_coords, lambertian_color, accumulated_color);
          }
        }

        accumulated_color = DirectX::XMVectorMultiplyAdd(
            ambient_color, DirectX::g_XMOne, accumulated_color);
        result_color = DirectX::XMVectorSaturate(accumulated_color);

        if (reflection_visibility == ReflectionVisibility::Visible) {
          TraceReflectionRay(result_color, meshes, mesh_index, face_index,
                             intersection_point, world_direction,
                             surface_normal);
        }

        closest_distance = intersection_result->z;
      }
    }
  }

  return result_color;
}
