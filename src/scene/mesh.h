#pragma once

#include <DirectXMath.h>

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace scene {
using Mesh =
    std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>;

Mesh LoadCube();

Mesh LoadOctahedron();

Mesh LoadRectangle();
}  // namespace scene
