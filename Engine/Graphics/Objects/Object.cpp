#include "Object.h"

Object::Object(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, const uint32_t& textureIndex, const uint32_t& modelIndex)
	: Entity(position, rotation, scale), m_index(0), m_textureIndex(textureIndex), m_modelIndex(modelIndex)
{

}

Object::~Object()
{

}
uint32_t Object::GetIndex() const
{
	return m_index;
}

void Object::SetModel(const uint32_t& modelIndex)
{
	m_modelIndex = modelIndex;
}

uint32_t Object::GetModelIndex() const
{
	return m_modelIndex;
}

void Object::SetTexture(const uint32_t& index)
{
	m_textureIndex = index;
}

uint32_t Object::GetTextureIndex() const
{
	return m_textureIndex;
}
