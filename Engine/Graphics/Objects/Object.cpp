#include "Object.h"

Object::Object()
{

}

Object::~Object()
{

}

void Object::SetPosition(const glm::vec3& newPosition)
{
	m_position = newPosition;
}

glm::vec3 Object::GetPosition() const
{
	return m_position;
}

void Object::SetModelIndex(const uint32_t& modelIndex)
{
	m_modelIndex = modelIndex;
}

uint32_t Object::GetModelIndex() const
{
	return m_modelIndex;
}
