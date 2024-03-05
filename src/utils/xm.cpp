#include "xm.h"

#include <algorithm>
#include <ranges>

void utils::xm::triangle::Load(DirectX::XMVECTOR& out_a,
                               DirectX::XMVECTOR& out_b,
                               DirectX::XMVECTOR& out_c,
                               std::span<const DirectX::XMFLOAT3A> vertices,
                               DirectX::XMINT3 face) {
  out_a = DirectX::XMLoadFloat3A(&vertices[face.x]);
  out_b = DirectX::XMLoadFloat3A(&vertices[face.y]);
  out_c = DirectX::XMLoadFloat3A(&vertices[face.z]);
}

DirectX::XMVECTOR utils::xm::triangle::GetBarycentrics(
    DirectX::FXMVECTOR a, DirectX::FXMVECTOR b, DirectX::FXMVECTOR c,
    DirectX::CXMVECTOR point) {
  const auto xm_ab = DirectX::XMVectorSubtract(b, a);
  const auto xm_ac = DirectX::XMVectorSubtract(c, a);
  const auto xm_ap = DirectX::XMVectorSubtract(point, a);
  const auto xm_abc = DirectX::XMVector2Cross(xm_ab, xm_ac);
  const auto xm_apc = DirectX::XMVector2Cross(xm_ap, xm_ac);
  const auto xm_abp = DirectX::XMVector2Cross(xm_ab, xm_ap);
  const auto xm_abc_reciprocal = DirectX::XMVectorReciprocal(xm_abc);
  const auto xm_beta = DirectX::XMVectorMultiply(xm_apc, xm_abc_reciprocal);
  const auto xm_gamma = DirectX::XMVectorMultiply(xm_abp, xm_abc_reciprocal);
  const auto beta = DirectX::XMVectorGetX(xm_beta);
  const auto gamma = DirectX::XMVectorGetX(xm_gamma);
  const auto alpha = 1.0f - beta - gamma;
  return DirectX::XMVectorSet(alpha, beta, gamma, alpha + beta + gamma);
}

bool utils::xm::triangle::IsPointInside(DirectX::FXMVECTOR barycentrics) {
  return DirectX::XMVector4LessOrEqual(barycentrics, DirectX::g_XMOne) &&
         DirectX::XMVector4GreaterOrEqual(barycentrics,
                                          DirectX::XMVectorZero());
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
