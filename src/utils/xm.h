#pragma once

#include <DirectXMath.h>

#include <array>
#include <functional>
#include <optional>
#include <span>

namespace utils::xm {
namespace scalar {
const float kEpsilon = DirectX::XMVectorGetX(DirectX::g_XMEpsilon);
}  // namespace scalar

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

inline DirectX::XMVECTOR Load(DirectX::XMFLOAT3A v) {
  return DirectX::XMLoadFloat3A(&v);
}

inline DirectX::XMFLOAT3A Transform(DirectX::XMFLOAT3A v,
                                    DirectX::FXMMATRIX xm_m) {
  return Store(DirectX::XMVector3Transform(Load(v), xm_m));
}

DirectX::XMFLOAT3A Clamp(DirectX::XMFLOAT3A v, float min_value,
                         float max_value);
}  // namespace float3a

namespace vector3 {
inline float CalculateTripleProduct(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b,
                                    DirectX::FXMVECTOR c) {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3Dot(DirectX::XMVector3Cross(a, b), c));
}
}  // namespace vector3

namespace triangle {
inline DirectX::XMVECTOR GetSurfaceNormal(DirectX::FXMVECTOR a,
                                          DirectX::FXMVECTOR b,
                                          DirectX::FXMVECTOR c) {
  return DirectX::XMVector3Cross(DirectX::XMVectorSubtract(b, a),
                                 DirectX::XMVectorSubtract(c, a));
}

void Load(DirectX::XMVECTOR& out_a, DirectX::XMVECTOR& out_b,
          DirectX::XMVECTOR& out_c,
          std::span<const DirectX::XMFLOAT3A> vertices, DirectX::XMINT3 face);

// Returns `beta`, `gamma` and `t` (distance).
std::optional<DirectX::XMFLOAT3A> Intersect(DirectX::FXMVECTOR a,
                                            DirectX::FXMVECTOR b,
                                            DirectX::FXMVECTOR c,
                                            DirectX::FXMVECTOR o,
                                            DirectX::FXMVECTOR d);

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

inline DirectX::XMVECTOR CalculateDirection(DirectX::FXMVECTOR a,
                                            DirectX::FXMVECTOR b) {
  return DirectX::XMVector3NormalizeEst(DirectX::XMVectorSubtract(b, a));
}
}  // namespace ray
}  // namespace utils::xm
