#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Entity.h"

class Object : public Entity
{
	friend class ObjectManager;
public:
	Object(const glm::vec3& position = {}, const glm::quat& rotation = {}, const glm::vec3& scale = {}, const uint32_t& modelIndex = 0, const uint32_t& textureIndex = 0);
	~Object();

	uint32_t GetIndex() const;

	void SetModel(const uint32_t& index);
	uint32_t GetModelIndex() const;

	void SetTexture(const uint32_t& index);
	uint32_t GetTextureIndex() const;
private:
	uint32_t m_index;
	uint32_t m_modelIndex;
	uint32_t m_textureIndex;
};

typedef std::shared_ptr<Object> pObject;