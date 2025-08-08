#pragma once

#include "../Models/Model.h"

class Object
{
public:
	Object();
	~Object();
	void SetPosition(const glm::vec3& newPosition);
	glm::vec3 GetPosition() const;
	void SetModelIndex(const uint32_t& modelIndex);
	uint32_t GetModelIndex() const;
private:
	uint32_t m_modelIndex = 0;
	
	glm::vec3 m_position = {};
};