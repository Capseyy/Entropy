#include "Camera.h"

Camera::Camera()
{
	this->pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
	this->posVector = XMLoadFloat3(&this->pos);

	this->rot = XMFLOAT3(0.0f, 0.0f, 0.0f);

	this->rotVector = XMLoadFloat3(&this->rot);
	this->UpdateViewMatrix();
}

static float Clamp(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}


void Camera::SetProjectionValues(float fovDegrees, float aspectRatio, float nearZ, float farZ)
{
	this->fov = fovDegrees;
	this->aspectRatio = aspectRatio;
	this->nearZ = nearZ;
	this->farZ = farZ;
	float fovRadians = (fovDegrees / 360.0f) * XM_2PI;
	this->projectionMatrix = XMMatrixPerspectiveFovLH(fovRadians, aspectRatio, nearZ, farZ);
}

const XMMATRIX& Camera::GetViewMatrix() const
{
	return this->viewMatrix;
}

const XMMATRIX& Camera::GetProjectionMatrix() const
{
	return this->projectionMatrix;
}

const XMVECTOR& Camera::GetPositionVector() const
{
	return this->posVector;
}

const XMFLOAT3& Camera::GetPositionFloat3() const
{
	return this->pos;
}

const XMVECTOR& Camera::GetRotationVector() const
{
	return this->rotVector;
}

const XMFLOAT3& Camera::GetRotationFloat3() const
{
	return this->rot;
}

void Camera::SetPosition(const XMVECTOR& pos)
{
	XMStoreFloat3(&this->pos, pos);
	this->posVector = pos;
	this->UpdateViewMatrix();
}

void Camera::SetPosition(float x, float y, float z)
{
	this->pos = XMFLOAT3(x, y, z);
	this->posVector = XMLoadFloat3(&this->pos);
	this->UpdateViewMatrix();
}

void Camera::AdjustPosition(const XMVECTOR& pos)
{
	this->posVector += pos;
	XMStoreFloat3(&this->pos, this->posVector);
	this->UpdateViewMatrix();
}

void Camera::AdjustPosition(float x, float y, float z)
{
	this->pos.x += x;
	this->pos.y += y;
	this->pos.z += z;
	this->posVector = XMLoadFloat3(&this->pos);
	this->UpdateViewMatrix();
}

void Camera::SetRotation(const XMVECTOR& rot)
{
	this->rotVector = rot;
	XMStoreFloat3(&this->rot, rot);
	this->UpdateViewMatrix();
}

void Camera::SetRotation(float x, float y, float z)
{
	this->rot = XMFLOAT3(x, y, z);
	this->rotVector = XMLoadFloat3(&this->rot);
	this->UpdateViewMatrix();
}

void Camera::AdjustRotation(const XMVECTOR& rot)
{
	this->rotVector += rot;
	XMStoreFloat3(&this->rot, this->rotVector);
	this->UpdateViewMatrix();
}

void Camera::AdjustRotation(float x, float y, float z)
{
	this->rot.x += x;
	this->rot.y += y;
	this->rot.z -= z;
	this->rotVector = XMLoadFloat3(&this->rot);
	this->UpdateViewMatrix();
}

void Camera::SetLookAtPos(XMFLOAT3 lookAtPos)
{
	if (lookAtPos.x == this->pos.x && lookAtPos.y == this->pos.y && lookAtPos.z == this->pos.z)
		return;

	XMFLOAT3 d = { lookAtPos.x - this->pos.x, lookAtPos.y - this->pos.y, lookAtPos.z - this->pos.z };

	float pitch = 0.0f;
	float xy = sqrtf(d.x * d.x + d.y * d.y);
	if (d.z != 0.0f || xy != 0.0f) pitch = atan2f(d.z, xy);

	float yawZ = atan2f(d.x, d.y);   // yaw around Z
	this->SetRotation(pitch, 0.0f, yawZ);
}


const XMVECTOR& Camera::GetForwardVector()
{
	return this->vec_forward;
}

const XMVECTOR& Camera::GetRightVector()
{
	return this->vec_right;
}

const XMVECTOR& Camera::GetBackwardVector()
{
	return this->vec_backward;
}

const XMVECTOR& Camera::GetLeftVector()
{
	return this->vec_left;
}

void Camera::UpdateViewMatrix()
{
	
	const float eps = 0.001f;
	if (this->rot.x > XM_PIDIV2 - eps) this->rot.x = XM_PIDIV2 - eps;
	if (this->rot.x < -XM_PIDIV2 + eps) this->rot.x = -XM_PIDIV2 + eps;

	const float pitch = this->rot.x;
	const float yaw = this->rot.z;

	XMVECTOR forward = XMVectorSet(
		sinf(yaw) * cosf(pitch),   // x
		cosf(yaw) * cosf(pitch),   // y
		sinf(pitch),               // z
		0.0f
	);
	forward = XMVector3Normalize(forward);

	const XMVECTOR worldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, worldUp));
	XMVECTOR up = XMVector3Normalize(XMVector3Cross(right, forward));

	XMVECTOR camTarget = XMVectorAdd(this->posVector, forward);
	XMMATRIX view = XMMatrixLookAtLH(this->posVector, camTarget, up);

	static const XMMATRIX MirrorX = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	this->viewMatrix = MirrorX * view;


	XMVECTOR flatForward = XMVector3Normalize(XMVectorSet(sinf(yaw), cosf(yaw), 0.0f, 0.0f));
	XMVECTOR flatRight = XMVector3Normalize(XMVector3Cross(flatForward, worldUp));


	this->vec_forward = forward;
	this->vec_backward = XMVectorNegate(forward);
	this->vec_right = XMVectorNegate(flatRight);
	this->vec_left = flatRight;
}


float Camera::GetSpeed()
{
	return this->CameraSpeed;
}

float Camera::SetSpeed(float speedScale)
{
	return this->CameraSpeed = speed_base * speedScale;
}