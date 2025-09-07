#include "Camera.h"

Camera::Camera() 
{
	pos = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	posVector = DirectX::XMLoadFloat3(&pos);
	rot = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	rotVector = DirectX::XMLoadFloat3(&rot);
	UpdateViewMatrix();
}

void Camera::SetProjectionValues(float fovDegrees, float aspectRatio, float nearZ, float farZ) 
{
	float fovRadians = (fovDegrees / 360.0f) * DirectX::XM_2PI;
	projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(fovRadians, aspectRatio, nearZ, farZ);
}

const DirectX::XMMATRIX& Camera::GetViewMatrix() const 
{
	return viewMatrix;
}

const DirectX::XMMATRIX& Camera::GetProjectionMatrix() const 
{
	return projectionMatrix;
}

const DirectX::XMVECTOR& Camera::GetPositionVector() const 
{
	return posVector;
}

const DirectX::XMFLOAT3& Camera::GetPositionFloat3() const 
{
	return pos;
}

const DirectX::XMVECTOR& Camera::GetRotationVector() const 
{
	return rotVector;
}

const DirectX::XMFLOAT3& Camera::GetRotationFloat3() const 
{
	return rot;
}

void Camera::SetPosition(const DirectX::XMVECTOR& pos) 
{
	DirectX::XMStoreFloat3(&this->pos, pos);
	posVector = pos;
	UpdateViewMatrix();
}

void Camera::SetPosition(float x, float y, float z) 
{
	pos = DirectX::XMFLOAT3{ x, y, z };
	posVector = DirectX::XMLoadFloat3(&pos);
	UpdateViewMatrix();
}

void Camera::AdjustPosition(const DirectX::XMVECTOR& pos) 
{
    posVector = DirectX::XMVectorAdd(posVector, pos);
    DirectX::XMStoreFloat3(&this->pos, posVector);
    UpdateViewMatrix();
}

void Camera::AdjustPosition(float x, float y, float z) 
{
	pos.x += x;
	pos.y += y;
	pos.z += z;
	posVector = DirectX::XMLoadFloat3(&pos);
	UpdateViewMatrix();
}

void Camera::SetRotation(const DirectX::XMVECTOR& rot) 
{
	DirectX::XMStoreFloat3(&this->rot, rot);
	rotVector = rot;
	UpdateViewMatrix();
}

void Camera::SetRotation(float x, float y, float z) 
{
	rot = DirectX::XMFLOAT3{ x, y, z };
	rotVector = DirectX::XMLoadFloat3(&rot);
	UpdateViewMatrix();
}

void Camera::AdjustRotation(const DirectX::XMVECTOR& rot) 
{
	rotVector = DirectX::XMVectorAdd(rotVector, rot);
	DirectX::XMStoreFloat3(&this->rot, rotVector);
	UpdateViewMatrix();
}

void Camera::AdjustRotation(float x, float y, float z) 
{
	rot.x += x;
	rot.y += y;
	rot.z += z;
	rotVector = DirectX::XMLoadFloat3(&rot);
	UpdateViewMatrix();
}

void Camera::UpdateViewMatrix() 
{
	// Calculate the view matrix
	DirectX::XMMATRIX camRotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
	// Apply rotations
	DirectX::XMVECTOR camTarget = DirectX::XMVector3TransformCoord(DEFAULT_FORWARD_VECTOR, camRotationMatrix);
	
	camTarget = DirectX::XMVectorAdd(posVector, camTarget);
	// Calculate the lookAt point
	DirectX::XMVECTOR upDir = DirectX::XMVector3TransformCoord(DEFAULT_UP_VECTOR, camRotationMatrix);
	// Create the view matrix
	viewMatrix = DirectX::XMMatrixLookAtLH(posVector, camTarget, upDir);

	DirectX::XMMATRIX vecRotaionMatrix = DirectX::XMMatrixRotationRollPitchYaw(0.0f,rot.y,0.0f);
	vec_forward = DirectX::XMVector3TransformCoord(DEFAULT_FORWARD_VECTOR, vecRotaionMatrix);
	vec_backward = DirectX::XMVector3TransformCoord(DEFAULT_BACKWARD_VECTOR, vecRotaionMatrix);
	vec_left = DirectX::XMVector3TransformCoord(DEFAULT_LEFT_VECTOR, vecRotaionMatrix);
	vec_right = DirectX::XMVector3TransformCoord(DEFAULT_RIGHT_VECTOR, vecRotaionMatrix);

}

void Camera::SetLookAtPos(DirectX::XMFLOAT3 lookAtPos)
{
	if (lookAtPos.x == this->pos.x && lookAtPos.y == this->pos.y && lookAtPos.z == this->pos.z)
		return;

	lookAtPos.x = this->pos.x - lookAtPos.x;
	lookAtPos.y = this->pos.y - lookAtPos.y;
	lookAtPos.z = this->pos.z - lookAtPos.z;

	float pitch = 0.0f;
	if (lookAtPos.y != 0.0f)
	{
		const float distance = sqrt(lookAtPos.x * lookAtPos.x + lookAtPos.y * lookAtPos.y + lookAtPos.z * lookAtPos.z);
		pitch = atan(lookAtPos.y / distance);
	}

	float yaw = 0.0f;
	if (lookAtPos.x != 0.0f)
	{
		yaw = atan(lookAtPos.x / lookAtPos.z);
	}
	if (lookAtPos.z > 0)
		yaw += DirectX::XM_PI;

	this->SetRotation(pitch, yaw, 0.0f);
}

const DirectX::XMVECTOR & Camera::GetRightVector()
{
	return this->vec_right;
}

const DirectX::XMVECTOR& Camera::GetLeftVector()
{
	return this->vec_left;
}

const DirectX::XMVECTOR& Camera::GetForwardVector()
{
	return this->vec_forward;
}

const DirectX::XMVECTOR& Camera::GetBackwardVector()
{
	return this->vec_backward;
}