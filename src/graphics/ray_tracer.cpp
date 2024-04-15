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
                                 size_t current_mesh_index,
                                 size_t current_face_index, size_t mesh_index,
                                 size_t face_index,
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
                       size_t current_mesh_index, size_t current_face_index,
                       DirectX::FXMVECTOR intersection_point,
                       const DirectX::XMFLOAT3A& light_position) {
  constexpr float kShadowIntensity = 0.0625f;
  float total_intensity = 0.0f;

  auto shadow_direction = utils::xm::ray::CalculateDirection(
      intersection_point, utils::xm::float3a::Load(light_position));

  // Iterate through meshes and faces.
  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    if (mesh_index == current_mesh_index) continue;

    const auto& mesh = meshes[mesh_index];
    for (size_t face_index = 0; face_index < mesh.second.size(); ++face_index) {
      if (mesh_index == current_mesh_index && face_index == current_face_index)
        continue;

      // Check for intersection.
      if (IntersectsWithShadow(meshes, current_mesh_index, current_face_index,
                               mesh_index, face_index, intersection_point,
                               light_position)) {
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
inline bool IntersectsWithReflection(
    DirectX::XMVECTOR& final_color, const std::span<model::Mesh>& meshes,
    size_t current_mesh_index, size_t current_face_index, size_t mesh_index,
    size_t face_index, DirectX::FXMVECTOR intersection_point,
    DirectX::FXMVECTOR incident_direction, DirectX::FXMVECTOR surface_normal) {
  DirectX::XMVECTOR vertex_a{};
  DirectX::XMVECTOR vertex_b{};
  DirectX::XMVECTOR vertex_c{};
  utils::xm::triangle::Load(vertex_a, vertex_b, vertex_c,
                            meshes[mesh_index].first,
                            meshes[mesh_index].second[face_index]);
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

    constexpr float reflectivity = 0.95f;
    final_color = DirectX::XMVectorSaturate(
        DirectX::XMVectorLerp(final_color, reflection_color, reflectivity));
    return true;
  }

  return false;
}

// Trace the reflection ray and calculate reflection color.
inline void TraceReflectionRay(DirectX::XMVECTOR& final_color,
                               const std::span<model::Mesh>& meshes,
                               size_t current_mesh_index,
                               size_t current_face_index,
                               DirectX::FXMVECTOR intersection_point,
                               DirectX::FXMVECTOR incident_direction,
                               DirectX::FXMVECTOR surface_normal) {
  for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
    // Skip the current mesh.
    if (mesh_index == current_mesh_index) {
      continue;
    }

    for (size_t face_index = 0; face_index < meshes[mesh_index].second.size();
         ++face_index) {
      // Check for reflection intersection.
      if (IntersectsWithReflection(final_color, meshes, current_mesh_index,
                                   current_face_index, mesh_index, face_index,
                                   intersection_point, incident_direction,
                                   surface_normal)) {
        break;
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
                IsShadowed(shadow_color, meshes, mesh_index, face_index,
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
          TraceReflectionRay(color, meshes, mesh_index, face_index,
                             world_intersection, xm_world_direction,
                             xm_surface_normal);
        }

        closest_distance = result->z;
      }
    }
  }

  return color;
}
