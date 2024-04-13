#pragma once

#include <DirectXMath.h>

#include <span>
#include <vector>

namespace graphics {
class MeshView {
 public:
  inline MeshView(std::span<DirectX::XMFLOAT3A> vertices,
                  std::span<DirectX::XMINT3> indices)
      : vertices_(vertices), indices_(indices) {}

  inline MeshView& Translate(float x, float y, float z) {
    DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(x, y, z);
    ApplyTransform(translation);
    return *this;
  }

  inline MeshView& Rotate(float roll, float pitch, float yaw) {
    DirectX::XMMATRIX rotation =
        DirectX::XMMatrixRotationRollPitchYaw(roll, pitch, yaw);
    ApplyTransform(rotation);
    return *this;
  }

  inline MeshView& Scale(float x, float y, float z) {
    DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(x, y, z);
    ApplyTransform(scaling);
    return *this;
  }

 private:
  inline void ApplyTransform(DirectX::FXMMATRIX transform) {
    for (auto& vertex : vertices_) {
      auto pos = DirectX::XMLoadFloat3A(&vertex);
      pos = DirectX::XMVector3Transform(pos, transform);
      DirectX::XMStoreFloat3A(&vertex, pos);
    }
  }

  std::span<DirectX::XMFLOAT3A> vertices_;
  std::span<DirectX::XMINT3> indices_;
};
}  // namespace graphics
