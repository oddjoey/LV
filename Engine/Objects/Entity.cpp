#include "Entity.h"

Entity::Entity(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
	: m_position(position), m_rotation(rotation), m_scale(scale)
{
}

Entity::~Entity()
{
}

Entity* Entity::SetPosition(const glm::vec3& newPosition)
{
	m_position = newPosition;
	return this;
}

glm::vec3 Entity::GetPosition() const
{
	return m_position;
}

Entity* Entity::SetRotation(const glm::quat& newRotation)
{
	m_rotation = newRotation;
	m_angles = glm::eulerAngles(newRotation);
	return this;
}

glm::quat Entity::GetRotation() const
{
	return m_rotation;
}

Entity* Entity::SetScale(const glm::vec3& scale)
{
	m_scale = scale;
	return this;
}

glm::vec3 Entity::GetScale() const
{
	return m_scale;
}

Entity* Entity::SetAngle(const glm::vec3& angle)
{
	m_angles = angle;
	m_rotation = glm::quat(glm::radians(angle));
	return this;
}

glm::vec3 Entity::GetAngles() const
{
	return glm::degrees(glm::eulerAngles(m_rotation));
}

glm::mat4 Entity::GetModelMatrix() const
{
	glm::mat4 translation = glm::translate(glm::mat4(1.f), m_position);
	glm::mat4 rotation = glm::mat4_cast(m_rotation);
	glm::mat4 scale = glm::scale(glm::mat4(1.f), m_scale);
	return translation * rotation * scale;
}
