#pragma once

#include <DirectXMath.h>

#include <array>
#include <functional>
#include <span>

namespace utils::xm {
std::span<DirectX::XMFLOAT3A> ApplyTransform(
    std::function<DirectX::XMFLOAT3A(const DirectX::XMFLOAT3A&)> transform,
    std::span<DirectX::XMFLOAT3A> output,
    std::span<const DirectX::XMFLOAT3A> input);

namespace float3a {
inline DirectX::XMFLOAT3A Store(DirectX::FXMVECTOR xm_v) {
  DirectX::XMFLOAT3A result{};
  DirectX::XMStoreFloat3A(&result, xm_v);
  return result;
}
}  // namespace float3a

namespace triangle {
inline std::array<DirectX::XMFLOAT3A, 3> Assemble(
    std::span<const DirectX::XMFLOAT3A> vertices, DirectX::XMINT3 face) {
  return {vertices[face.x], vertices[face.y], vertices[face.z]};
}

void Load(DirectX::XMVECTOR& out_a, DirectX::XMVECTOR& out_b,
          DirectX::XMVECTOR& out_c,
          std::span<const DirectX::XMFLOAT3A> vertices, DirectX::XMINT3 face);

DirectX::XMVECTOR GetBarycentrics(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b,
                                  DirectX::FXMVECTOR c,
                                  DirectX::CXMVECTOR point);

bool IsPointInside(DirectX::FXMVECTOR barycentrics);

DirectX::XMVECTOR Interpolate(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b,
                              DirectX::FXMVECTOR c,
                              DirectX::CXMVECTOR barycentrics);
}  // namespace triangle

namespace ray {
inline DirectX::XMVECTOR At(DirectX::FXMVECTOR origin,
                            DirectX::FXMVECTOR direction, float t) {
  return DirectX::XMVectorMultiplyAdd(direction, DirectX::XMVectorReplicate(t),
                                      origin);
}
}  // namespace ray
}  // namespace utils::xm
