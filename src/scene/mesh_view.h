#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <algorithm>
#include <span>
#include <vector>

namespace scene {
class MeshView {
 public:
  inline MeshView(std::span<DirectX::XMFLOAT3A> vertices,
                  std::span<DirectX::XMINT3> faces)
      : vertices_(vertices), faces_(faces) {}

  inline MeshView& Translate(float x = {}, float y = {}, float z = {}) {
    ApplyTransform(DirectX::XMMatrixTranslation(x, y, z));
    return *this;
  }

  inline MeshView& Rotate(float roll = {}, float pitch = {}, float yaw = {}) {
    ApplyTransform(DirectX::XMMatrixRotationRollPitchYaw(roll, pitch, yaw));
    return *this;
  }

  inline MeshView& Scale(float x = 1.0f, float y = 1.0f, float z = 1.0f) {
    ApplyTransform(DirectX::XMMatrixScaling(x, y, z));
    return *this;
  }

  inline MeshView& FlipWindingOrder() {
    for (auto& face : faces_) {
      std::swap(face.y, face.z);
    }
    return *this;
  }

  inline MeshView& Normalize() {
    // Compute the bounding box of the mesh.
    DirectX::BoundingBox bbox{};
    DirectX::BoundingBox::CreateFromPoints(
        bbox, vertices_.size(), &vertices_[0], sizeof(DirectX::XMFLOAT3A));

    // Calculate the scale factor to normalize the dimensions to fit within a
    // unit cube.
    DirectX::XMVECTOR scale = DirectX::XMVectorReciprocal(
        DirectX::XMVectorMax(DirectX::XMLoadFloat3(&bbox.Extents),
                             DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)));

    // Normalize each vertex.
    for (auto& vertex : vertices_) {
      // Translate the vertex to the center of the bounding box.
      DirectX::XMVECTOR v = DirectX::XMVectorSubtract(
          DirectX::XMLoadFloat3A(&vertex), DirectX::XMLoadFloat3(&bbox.Center));

      // Scale the vertex to fit within a unit cube.
      v = DirectX::XMVectorMultiply(v, scale);

      DirectX::XMStoreFloat3A(&vertex, v);
    }

    return *this;
  }

  inline MeshView& SortFacesByAvgZ() {
    std::sort(faces_.begin(), faces_.end(), [this](auto& face_1, auto& face_2) {
      constexpr auto kOneThird = 1.0f / 3.0f;
      const float avg_z1 = (vertices_[static_cast<size_t>(face_1.x)].z +
                            vertices_[static_cast<size_t>(face_1.y)].z +
                            vertices_[static_cast<size_t>(face_1.z)].z) *
                           kOneThird;
      const float avg_z2 = (vertices_[static_cast<size_t>(face_2.x)].z +
                            vertices_[static_cast<size_t>(face_2.y)].z +
                            vertices_[static_cast<size_t>(face_2.z)].z) *
                           kOneThird;
      return avg_z1 < avg_z2;
    });

    return *this;
  }

 private:
  inline MeshView& ApplyTransform(DirectX::CXMMATRIX transform) {
    for (auto& vertex : vertices_) {
      DirectX::XMStoreFloat3A(
          &vertex, DirectX::XMVector3Transform(DirectX::XMLoadFloat3A(&vertex),
                                               transform));
    }
    return *this;
  }

  std::span<DirectX::XMFLOAT3A> vertices_;
  std::span<DirectX::XMINT3> faces_;
};
}  // namespace scene
