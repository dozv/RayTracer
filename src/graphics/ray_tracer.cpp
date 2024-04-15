#include "ray_tracer.h"

#include <algorithm>
#include <ranges>

namespace {
inline float CalculateDistance(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b) {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3LengthEst(DirectX::XMVectorSubtract(b, a)));
}

struct MeshParams {
  std::span<model::Mesh> meshes;
  unsigned int current_mesh_index;
  unsigned int current_face_index;
};

inline bool TraceShadowRays(DirectX::XMVECTOR& color, const MeshParams& params,
                            DirectX::FXMVECTOR world_intersection,
                            DirectX::FXMVECTOR world_shadow_direction,
                            DirectX::FXMVECTOR light_position) {
  bool in_shadow = false;

  for (const auto& mesh : params.meshes) {
    if (&mesh == &params.meshes[params.current_mesh_index]) continue;

    for (const auto& face : mesh.second) {
      if (&face == &params.meshes[params.current_mesh_index]
                        .second[params.current_face_index])
        continue;

      DirectX::XMVECTOR shadow_a{};
      DirectX::XMVECTOR shadow_b{};
      DirectX::XMVECTOR shadow_c{};

      utils::xm::triangle::Load(shadow_a, shadow_b, shadow_c, mesh.first, face);
      const auto offset_world_origin =
          DirectX::XMVectorAdd(world_intersection, DirectX::g_XMEpsilon);
      const auto shadow_result = utils::xm::triangle::Intersect(
          shadow_a, shadow_b, shadow_c, offset_world_origin,
          world_shadow_direction);

      if (shadow_result.has_value() && shadow_result->z > 0.0f &&
          shadow_result->z <
              CalculateDistance(light_position, world_intersection)) {
        in_shadow = true;
        color = DirectX::XMVectorReplicate(0.0625f);  // Hint: no alpha channel.
        break;
      }
    }

    if (in_shadow) break;
  }

  return in_shadow;
}

inline void TraceReflectionRays(DirectX::XMVECTOR& color,
                                const MeshParams& params,
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

DirectX::XMVECTOR ray_tracer::TraceRays(bool& visible_shadows,
                                        bool& show_reflections,
                                        std::span<model::Mesh> meshes,
                                        DirectX::FXMVECTOR xm_world_direction,
                                        DirectX::FXMVECTOR xm_world_origin,
                                        DirectX::FXMVECTOR light_position) {
  float closest_distance = std::numeric_limits<float>::infinity();
  const auto ambient = DirectX::XMVectorReplicate(0.2f);
  DirectX::XMVECTOR color = DirectX::g_XMOne;

  for (auto mesh_index = 0U; mesh_index < meshes.size(); ++mesh_index) {
    for (auto face_index = 0U; face_index < meshes[mesh_index].second.size();
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

        DirectX::XMVECTOR world_shadow_direction =
            utils::xm::ray::GetNormalizedDirectionFromPoints(world_intersection,
                                                             light_position);

        DirectX::XMVECTOR barycentrics = DirectX::XMVectorSet(
            1.0f - result->x - result->y, result->x, result->y, 1.0f);

        // Trace shadow rays.
        bool in_shadow = false;
        if (visible_shadows) {
          in_shadow = TraceShadowRays(color, {meshes, mesh_index, face_index},
                                      world_intersection,
                                      world_shadow_direction, light_position);
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

          color = DirectX::XMVectorSaturate(DirectX::XMVectorMultiplyAdd(
              barycentrics, xm_lambertian, ambient));
        }  // if (not in shadow).

        // Trace reflection rays.
        if (show_reflections) {
          TraceReflectionRays(color, {meshes, mesh_index, face_index},
                              world_intersection, xm_world_direction,
                              xm_surface_normal);
        }  // if (show reflections).

        closest_distance = result->z;
      }  // if (intersection).
    }    // for-each face (triangle).
  }      // for-each mesh.

  return color;
}
