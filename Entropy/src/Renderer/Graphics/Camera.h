#pragma once
#include <DirectXMath.h>

class Camera {
public:
	Camera();

    void SetProjectionValues(float fovDegrees, float aspectRatio, float nearZ, float farZ);

    const DirectX::XMMATRIX& GetViewMatrix() const;
    const DirectX::XMMATRIX& GetProjectionMatrix() const;

    const DirectX::XMVECTOR& GetPositionVector() const;
    const DirectX::XMFLOAT3& GetPositionFloat3() const;
    const DirectX::XMVECTOR& GetRotationVector() const;
    const DirectX::XMFLOAT3& GetRotationFloat3() const;

    void SetPosition(const DirectX::XMVECTOR& pos);
    void SetPosition(float x, float y, float z);
    void AdjustPosition(const DirectX::XMVECTOR& pos);
    void AdjustPosition(float dx, float dy, float dz);

    void SetRotation(const DirectX::XMVECTOR& rot);
    void SetRotation(float pitch, float yaw, float roll);
    void AdjustRotation(const DirectX::XMVECTOR& delta);
    void AdjustRotation(float dpitch, float dyaw, float droll);

private:
    // Recalculate the view matrix from pos/rot
    void UpdateViewMatrix();
    // State
    DirectX::XMVECTOR posVector;
    DirectX::XMVECTOR rotVector;
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 rot;
    DirectX::XMMATRIX viewMatrix;
    DirectX::XMMATRIX projectionMatrix;

    // Safe static constants for default directions
    const DirectX::XMVECTOR DEFAULT_FORWARD_VECTOR = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const DirectX::XMVECTOR DEFAULT_UP_VECTOR = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
};