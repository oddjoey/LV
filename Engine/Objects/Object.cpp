#include "Object.h"

Object::Object(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, const uint32_t& modelIndex, const uint32_t& textureIndex)
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

Object* Object::SetTint(const glm::vec4& tint)
{
	m_tint = tint;
	return this;
}

glm::vec4 Object::GetTint() const
{
	return m_tint;
}

Object* Object::SetEmissive(const float& emissive)
{
	m_emissive = emissive;
	return this;
}

float Object::GetEmissive() const
{
	return m_emissive;
}

Object* Object::SetVisible(const bool& visible)
{
	m_visible = visible;
	return this;
}

bool Object::IsVisible() const
{
	return m_visible;
}

Object* Object::SetPickable(const bool& pickable)
{
	m_pickable = pickable;
	return this;
}

bool Object::IsPickable() const
{
	return m_pickable;
}

Object* Object::SetCastsShadows(const bool& castsShadows)
{
	m_castsShadows = castsShadows;
	return this;
}

bool Object::CastsShadows() const
{
	return m_castsShadows;
}

Object* Object::SetCastsLampShadows(const bool& castsLampShadows)
{
	m_castsLampShadows = castsLampShadows;
	return this;
}

bool Object::CastsLampShadows() const
{
	return m_castsLampShadows;
}

Object* Object::SetShading(const ObjectShading& shading)
{
	m_shading = shading;
	return this;
}

ObjectShading Object::GetShading() const
{
	return m_shading;
}

bool Object::IsTransparent() const
{
	return m_tint.a < 0.999f;
}
