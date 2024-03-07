#include "xm.h"

#include <algorithm>
#include <ranges>

void utils::xm::triangle::Load(DirectX::XMVECTOR& out_a,
                               DirectX::XMVECTOR& out_b,
                               DirectX::XMVECTOR& out_c,
                               std::span<const DirectX::XMFLOAT3A> vertices,
                               DirectX::XMINT3 face) {
  out_a = DirectX::XMLoadFloat3A(&vertices[static_cast<uint32_t>(face.x)]);
  out_b = DirectX::XMLoadFloat3A(&vertices[static_cast<uint32_t>(face.y)]);
  out_c = DirectX::XMLoadFloat3A(&vertices[static_cast<uint32_t>(face.z)]);
}

std::optional<DirectX::XMFLOAT3A> utils::xm::triangle::Intersect(
    DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, DirectX::FXMVECTOR c,
    DirectX::FXMVECTOR o, DirectX::FXMVECTOR d) {
  const auto ab = DirectX::XMVectorSubtract(b, a);
  const auto ac = DirectX::XMVectorSubtract(c, a);
  const auto minus_d = DirectX::XMVectorNegate(d);
  const float vol = utils::xm::vector3::CalculateTripleProduct(ab, ac, minus_d);
  if (DirectX::XMScalarNearEqual(vol, 0.0f, utils::xm::scalar::kEpsilon)) {
    return std::nullopt;
  }

  const auto ao = DirectX::XMVectorSubtract(o, a);
  const float vol_beta =
      utils::xm::vector3::CalculateTripleProduct(ao, ac, minus_d);

  const float beta = vol_beta / vol;
  if (beta < 0.0f || beta > 1.0f) {
    return std::nullopt;
  }

  const float vol_gamma =
      utils::xm::vector3::CalculateTripleProduct(ab, ao, minus_d);
  const float gamma = vol_gamma / vol;

  if (gamma < 0.0f || gamma > 1.0f || (beta + gamma) > 1.0f) {
    return std::nullopt;
  }

  const float vol_t = utils::xm::vector3::CalculateTripleProduct(ab, ac, ao);
  const float ray_param = vol_t / vol;
  if (ray_param < 0.0f) {
    return std::nullopt;
  }

  return DirectX::XMFLOAT3A{beta, gamma, ray_param};
}

DirectX::XMVECTOR utils::xm::triangle::Interpolate(
    DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, DirectX::FXMVECTOR c,
    DirectX::CXMVECTOR barycentrics) {
  const auto xm_alpha = DirectX::XMVectorSplatX(barycentrics);
  const auto xm_beta = DirectX::XMVectorSplatY(barycentrics);
  const auto xm_gamma = DirectX::XMVectorSplatZ(barycentrics);
  const auto scaled_a = DirectX::XMVectorMultiply(xm_alpha, a);
  const auto scaled_b = DirectX::XMVectorMultiply(xm_beta, b);
  const auto scaled_c = DirectX::XMVectorMultiply(xm_gamma, c);
  const auto sum_ab = DirectX::XMVectorAdd(scaled_a, scaled_b);
  const auto sum_abc = DirectX::XMVectorAdd(sum_ab, scaled_c);
  const auto interpolation = DirectX::XMVectorSetW(sum_abc, 1.0f);
  return interpolation;
}

std::span<DirectX::XMFLOAT3A> utils::xm::ApplyTransform(
    std::function<DirectX::XMFLOAT3A(const DirectX::XMFLOAT3A&)> transform,
    std::span<DirectX::XMFLOAT3A> output,
    std::span<const DirectX::XMFLOAT3A> input) {
  auto output_subspan = output.subspan(0, input.size());
  std::ranges::transform(input, output_subspan.begin(), transform);
  return output_subspan;
}
