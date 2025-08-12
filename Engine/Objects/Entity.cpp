#include "Entity.h"

Entity::Entity(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
	: m_position(position), m_rotation(rotation), m_scale(scale)
{
}

Entity::~Entity()
{
}

void Entity::SetPosition(const glm::vec3& newPosition)
{
	m_position = newPosition;
}

glm::vec3 Entity::GetPosition() const
{
	return m_position;
}

void Entity::SetRotation(const glm::quat& newRotation)
{
	m_rotation = newRotation;
}

glm::quat Entity::GetRotation() const
{
	return m_rotation;
}

void Entity::SetScale(const glm::vec3 scale)
{
	m_scale = scale;
}

glm::vec3 Entity::GetScale() const
{
	return m_scale;
}
