#pragma once

#include <span>

#include "../scene/mesh.h"
#include "../utils/xm.h"

namespace ray_tracer {
enum class ShadowVisibility { Visible, Hidden };
enum class ReflectionVisibility { Visible, Hidden };

DirectX::XMVECTOR TraceRays(
    ShadowVisibility shadow_visibility,
    ReflectionVisibility reflection_visibility, std::span<scene::Mesh> meshes,
    DirectX::FXMVECTOR world_direction, DirectX::FXMVECTOR world_origin,
    std::span<const DirectX::XMFLOAT3A> light_positions);
}  // namespace ray_tracer
