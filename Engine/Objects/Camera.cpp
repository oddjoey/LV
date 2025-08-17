#include "Camera.h"

Camera::Camera(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, const float& fov, const float& nearPlane, const float& farPlane)
	: Entity(position, rotation, scale), m_index(0), m_viewMatrix({}), m_fov(fov), m_nearPlane(nearPlane), m_farPlane(farPlane)
{
	SetRotation(rotation);
}

Camera::~Camera()
{
}

Camera* Camera::LookAt(const glm::vec3& position)
{
	auto viewMatrix = glm::lookAt(GetPosition(), position, glm::vec3(0.f, 1.f, 0.f));
	auto rotation = glm::quat_cast(glm::transpose(glm::mat3(viewMatrix)));
	SetRotation(rotation);
	return this;
}

glm::mat4 Camera::CalculateViewMatrix()
{
	glm::vec3 targetDirection = m_position + m_rotation * glm::vec3(0.f, 0.f, -1.f);
	m_viewMatrix = glm::lookAt(m_position, targetDirection, glm::vec3(0.f, 1.f, 0.f));
	return m_viewMatrix;
}

Camera* Camera::SetViewMatrix(const glm::mat4& viewMatrix)
{
	m_viewMatrix = viewMatrix;
	return this;
}

glm::mat4 Camera::GetViewMatrix() const
{
	return m_viewMatrix;
}

uint32_t Camera::GetIndex() const
{
	return m_index;
}

Camera* Camera::SetFov(const float& fov)
{
	m_fov = fov;
	return this;
}

float Camera::GetFov() const
{
	return m_fov;
}

Camera* Camera::SetNearPlane(const float& nearPlane)
{
	m_nearPlane = nearPlane;
	return this;
}

float Camera::GetNearPlane() const
{
	return m_nearPlane;
}

Camera* Camera::SetFarPlane(const float& farPlane)
{
	m_farPlane = farPlane;
	return this;
}

float Camera::GetFarPlane() const
{
	return m_farPlane;
}
