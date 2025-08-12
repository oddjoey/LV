#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Entity
{
public:
	Entity(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale);
	~Entity();

	void SetPosition(const glm::vec3& position);
	glm::vec3 GetPosition() const;

	virtual void SetRotation(const glm::quat& rotation);
	glm::quat GetRotation() const;

	void SetScale(const glm::vec3 scale);
	glm::vec3 GetScale() const;
private:
	glm::vec3 m_position = {};
	glm::quat m_rotation = {};
	glm::vec3 m_scale = {};
};