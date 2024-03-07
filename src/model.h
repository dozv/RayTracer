#pragma once

#include <DirectXMath.h>

#include <span>
#include <utility>
#include <vector>

namespace model {
inline void FlipWindingOrder(std::span<DirectX::XMINT3> in_out_faces) {
  for (auto& face : in_out_faces) {
    std::swap(face.y, face.z);
  }
}

std::pair<std::vector<DirectX::XMFLOAT3A>, std::vector<DirectX::XMINT3>>
LoadCube();
}  // namespace model
