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
	// Set every frame for a lot of things that aren't turning: skip the trig when nothing changed.
	if (newRotation == m_rotation)
		return this;
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

const glm::mat4& Entity::GetModelMatrix() const
{
	if (m_matrixValid && m_position == m_matrixPosition && m_rotation == m_matrixRotation && m_scale == m_matrixScale)
		return m_matrix;

	// The rotation's columns scaled, and the translation: the same as T * R * S without the multiplies.
	glm::mat3 rotation = glm::mat3_cast(m_rotation);
	m_matrix[0] = glm::vec4(rotation[0] * m_scale.x, 0.f);
	m_matrix[1] = glm::vec4(rotation[1] * m_scale.y, 0.f);
	m_matrix[2] = glm::vec4(rotation[2] * m_scale.z, 0.f);
	m_matrix[3] = glm::vec4(m_position, 1.f);
	m_matrixPosition = m_position;
	m_matrixRotation = m_rotation;
	m_matrixScale = m_scale;
	m_matrixValid = true;
	return m_matrix;
}
