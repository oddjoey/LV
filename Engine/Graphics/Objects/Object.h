#pragma once

#include "../Models/Model.h"

class Object
{
public:
	Object();
	~Object();
	void SetPosition(const glm::vec3& newPosition);
	glm::vec3 GetPosition() const;
	void SetModelIndex(const uint8_t& modelIndex);
	uint8_t GetModelIndex() const;
private:
	uint8_t m_modelIndex = 0;
	glm::vec3 m_position = {};
};