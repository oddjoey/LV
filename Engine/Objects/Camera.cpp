#include "Camera.h"

Camera::Camera(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, const float& fov, const float& nearPlane, const float& farPlane)
	: Entity(position, rotation, scale), m_index(0), m_viewMatrix({}), m_fov(fov), m_nearPlane(nearPlane), m_farPlane(farPlane)
{
	SetRotation(rotation);
}

Camera::~Camera()
{
}

void Camera::LookAt(const glm::vec3& position)
{
	m_viewMatrix = glm::lookAt(GetPosition(), position, glm::vec3(0.f, 0.f, 1.f));
	Entity::SetRotation(glm::quat_cast(glm::mat3(glm::inverse(m_viewMatrix))));
}

void Camera::SetViewMatrix(const glm::mat4& viewMatrix)
{
	m_viewMatrix = viewMatrix;
}

glm::mat4 Camera::GetViewMatrix() const
{
	return m_viewMatrix;
}

void Camera::SetRotation(const glm::quat& rotation)
{
	glm::vec3 targetPosition = GetPosition() + rotation * glm::vec3(0.f, -1.f, 0.f);
	m_viewMatrix = glm::lookAt(GetPosition(), targetPosition, glm::vec3(0.f, 0.f, 1.f));
	Entity::SetRotation(rotation);
}

uint32_t Camera::GetIndex() const
{
	return m_index;
}

void Camera::SetFov(const float& fov)
{
	m_fov = fov;
}

float Camera::GetFov() const
{
	return m_fov;
}

void Camera::SetNearPlane(const float& nearPlane)
{
	m_nearPlane = nearPlane;
}

float Camera::GetNearPlane() const
{
	return m_nearPlane;
}

void Camera::SetFarPlane(const float& farPlane)
{
	m_farPlane = farPlane;
}

float Camera::GetFarPlane() const
{
	return m_farPlane;
}
