#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <cmath>
#include <optional>
#include <span>

using Int3 = DirectX::XMINT3;
using Vector = DirectX::XMVECTOR;
using Matrix = DirectX::XMMATRIX;

namespace scalar {
const float kEpsilon = DirectX::XMVectorGetX(DirectX::g_XMEpsilon);
}

class Float2 : public DirectX::XMFLOAT2A {
 public:
  inline Float2() = default;

  inline Float2(Vector v) { DirectX::XMStoreFloat2A(this, v); }

  inline Float2(float x, float y) : DirectX::XMFLOAT2A(x, y) {}

  inline Vector Load() const { return DirectX::XMLoadFloat2A(this); }
};

class Float3 : public DirectX::XMFLOAT3A {
 public:
  inline Float3() = default;

  inline Float3(float x, float y, float z) : DirectX::XMFLOAT3A(x, y, z) {}

  inline Float3(Vector v) { DirectX::XMStoreFloat3A(this, v); }

  inline Vector Load() const { return DirectX::XMLoadFloat3A(this); }

  inline Float3 Transform(Matrix m) const {
    return Float3(DirectX::XMVector3Transform(Load(), m));
  }
};

namespace vector2 {
inline float CalculateWedgeProduct(Vector a, Vector b) {
  return DirectX::XMVectorGetX(DirectX::XMVector2Cross(a, b));
}
}  // namespace vector2

namespace vector3 {
inline float CalculateTripleProduct(Vector a, Vector b, Vector c) {
  return DirectX::XMVectorGetX(
      DirectX::XMVector3Dot(DirectX::XMVector3Cross(a, b), c));
}
}  // namespace vector3

namespace triangle {
inline float CalculateAverageZ(Vector a, const Vector b, Vector c) {
  const auto sum = DirectX::XMVectorAdd(DirectX::XMVectorAdd(a, b), c);
  constexpr auto kOneThird = 1.0f / 3.0f;
  return DirectX::XMVectorGetZ(sum) * kOneThird;
}

inline Vector CalculateSurfaceNormal(Vector a, Vector b, Vector c) {
  return DirectX::XMVector3Cross(DirectX::XMVectorSubtract(b, a),
                                 DirectX::XMVectorSubtract(c, a));
}

inline void Load(Vector& out_a, Vector& out_b, Vector& out_c,
                 std::span<const Float3> vertices, const Int3& face) {
  out_a = vertices[static_cast<uint32_t>(face.x)].Load();
  out_b = vertices[static_cast<uint32_t>(face.y)].Load();
  out_c = vertices[static_cast<uint32_t>(face.z)].Load();
}

inline Float2 CalculateBarycentricCoords(Vector a, Vector b, Vector c,
                                         Vector p) {
  const auto ab = DirectX::XMVectorSubtract(b, a);
  const auto ac = DirectX::XMVectorSubtract(c, a);
  const auto ap = DirectX::XMVectorSubtract(p, a);

  const float abc = DirectX::XMVectorGetX(DirectX::XMVector2Cross(ab, ac));
  const float beta =
      DirectX::XMVectorGetX(DirectX::XMVector2Cross(ab, ap)) / abc;
  const float gamma =
      DirectX::XMVectorGetX(DirectX::XMVector2Cross(ap, ac)) / abc;
  return {beta, gamma};
}

inline std::optional<Float3> Intersect(Vector a, Vector b, Vector c, Vector o,
                                       Vector d) {
                                       Vector d) {
                                         const auto ab =
                                             DirectX::XMVectorSubtract(b, a);
                                         const auto ac =
                                             DirectX::XMVectorSubtract(c, a);
                                         const auto minus_d =
                                             DirectX::XMVectorNegate(d);
                                         const float vol =
                                             vector3::CalculateTripleProduct(
                                                 ab, ac, minus_d);
                                         if (DirectX::XMScalarNearEqual(
                                                 vol, 0.0f, scalar::kEpsilon)) {
                                           return std::nullopt;
                                         }

                                         const auto ao =
                                             DirectX::XMVectorSubtract(o, a);
                                         const float vol_beta =
                                             vector3::CalculateTripleProduct(
                                                 ao, ac, minus_d);

                                         const float beta = vol_beta / vol;
                                         if (beta < 0.0f || beta > 1.0f) {
                                           return std::nullopt;
                                         }

                                         const float vol_gamma =
                                             vector3::CalculateTripleProduct(
                                                 ab, ao, minus_d);
                                         const float gamma = vol_gamma / vol;

                                         if (gamma < 0.0f || gamma > 1.0f ||
                                             (beta + gamma) > 1.0f) {
                                           return std::nullopt;
                                         }

                                         const float vol_t =
                                             vector3::CalculateTripleProduct(
                                                 ab, ac, ao);
                                         const float ray_param = vol_t / vol;
                                         if (ray_param < 0.0f) {
                                           return std::nullopt;
                                         }

                                         return Float3{beta, gamma, ray_param};
                                       }

                                       inline Vector Interpolate(
                                           Vector a, Vector b, Vector c,
                                           Vector barycentrics) {
                                         const auto xm_alpha =
                                             DirectX::XMVectorSplatX(
                                                 barycentrics);
                                         const auto xm_beta =
                                             DirectX::XMVectorSplatY(
                                                 barycentrics);
                                         const auto xm_gamma =
                                             DirectX::XMVectorSplatZ(
                                                 barycentrics);
                                         const auto scaled_a =
                                             DirectX::XMVectorMultiply(xm_alpha,
                                                                       a);
                                         const auto scaled_b =
                                             DirectX::XMVectorMultiply(xm_beta,
                                                                       b);
                                         const auto scaled_c =
                                             DirectX::XMVectorMultiply(xm_gamma,
                                                                       c);
                                         const auto sum_ab =
                                             DirectX::XMVectorAdd(scaled_a,
                                                                  scaled_b);
                                         const auto sum_abc =
                                             DirectX::XMVectorAdd(sum_ab,
                                                                  scaled_c);
                                         const auto interpolation =
                                             DirectX::XMVectorSetW(sum_abc,
                                                                   1.0f);
                                         return interpolation;
                                       }

                                       inline bool ShouldClipZ(
                                           DirectX::XMVECTOR a,
                                           DirectX::XMVECTOR b,
                                           DirectX::XMVECTOR c, float z_near,
                                           float z_far) {
                                         // Extract the z-values from the
                                         // vectors.
                                         float z1 = DirectX::XMVectorGetZ(a);
                                         float z2 = DirectX::XMVectorGetZ(b);
                                         float z3 = DirectX::XMVectorGetZ(c);

                                         // Check if any z-value is outside the
                                         // near and far clipping planes. In a
                                         // RH system, z-values increase from
                                         // the far plane to the near plane.
                                         if (z1 > z_near || z1 < z_far ||
                                             z2 > z_near || z2 < z_far ||
                                             z3 > z_near || z3 < z_far) {
                                           return true;  // The triangle should
                                                         // be clipped.
                                         }

                                         return false;  // The triangle should
                                                        // not be clipped.
                                       }
}  // namespace triangle

namespace ray {
inline Vector At(Vector origin, Vector direction, float t) {
  return DirectX::XMVectorMultiplyAdd(direction, DirectX::XMVectorReplicate(t),
                                      origin);
}
}  // namespace ray
