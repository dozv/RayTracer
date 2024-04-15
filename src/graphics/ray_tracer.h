#pragma once

#include <span>

#include "../utils/xm.h"
#include "model.h"

namespace ray_tracer {
DirectX::XMVECTOR TraceRays(
    bool& visible_shadows, bool& show_reflections,
    std::span<model::Mesh> meshes, DirectX::FXMVECTOR xm_world_direction,
    DirectX::FXMVECTOR xm_world_origin,
    std::span<const DirectX::XMFLOAT3A> light_positions);
}  // namespace ray_tracer
