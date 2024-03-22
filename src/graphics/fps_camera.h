#pragma once

#include <DirectXMath.h>

#include <cmath>

class FpsCamera {
 public:
  inline FpsCamera() : position(0.0f, 0.0f, 0.0f), pitch_(0.0f), yaw_(0.0f) {}

  inline void Move(float delta_forward, float delta_right) {
    DirectX::XMFLOAT3A move_direction{};
    DirectX::XMStoreFloat3A(
        &move_direction,
        DirectX::XMVectorAdd(
            DirectX::XMVectorScale(GetForwardVector(), delta_forward),
            DirectX::XMVectorScale(GetRightVector(), delta_right)));
    position.x += move_direction.x;
    position.y += move_direction.y;
    position.z += move_direction.z;
  }

  inline void Rotate(float delta_pitch, float delta_yaw) {
    pitch_ += delta_pitch;
    yaw_ = fmodf(yaw_ + delta_yaw, 360.0f);
    pitch_ = std::fmaxf(-89.0f, std::fminf(89.0f, pitch_));
  }

  inline DirectX::XMMATRIX GetWorldToCameraMatrix() const {
    return DirectX::XMMatrixLookToRH(DirectX::XMLoadFloat3(&position),
                                     GetForwardVector(), GetUpVector());
  }

  inline DirectX::XMMATRIX GetCameraToWorldMatrix() const {
    return DirectX::XMMatrixInverse(nullptr, GetWorldToCameraMatrix());
  }

  inline DirectX::XMFLOAT3A GetPosition() const { return position; }

 private:
  DirectX::XMFLOAT3A position;
  float pitch_;
  float yaw_;
  float roll_{};
  float dummy_{};

  inline DirectX::XMVECTOR GetForwardVector() const {
    const auto minus_unit_z = DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    return DirectX::XMVector3Normalize(DirectX::XMVector3Rotate(
        minus_unit_z, DirectX::XMQuaternionRotationRollPitchYaw(
                          DirectX::XMConvertToRadians(pitch_),
                          DirectX::XMConvertToRadians(yaw_), 0.0f)));
  }

  inline DirectX::XMVECTOR GetRightVector() const {
    const auto unit_y = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return DirectX::XMVector3Normalize(
        DirectX::XMVector3Cross(unit_y, GetForwardVector()));
  }

  inline DirectX::XMVECTOR GetUpVector() const {
    return DirectX::XMVector3Cross(GetForwardVector(), GetRightVector());
  }
};
