#pragma once

#include <DirectXMath.h>

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace model {
using Mesh =
    std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>;

using MeshView =
    std::pair<std::span<DirectX::XMFLOAT3A>, std::span<DirectX::XMINT3>>;

inline void FlipWindingOrder(std::span<DirectX::XMINT3> in_out_faces) {
  for (auto& face : in_out_faces) {
    std::swap(face.y, face.z);
  }
}

std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>
LoadCube();

std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>
LoadOctahedron();

std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>
LoadRectangle();
}  // namespace model
