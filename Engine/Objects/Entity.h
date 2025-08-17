#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

class Entity
{
	friend class IMGUIManager;
public:
	Entity(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale);
	~Entity();

	Entity* SetPosition(const glm::vec3& position);
	glm::vec3 GetPosition() const;

	Entity* SetRotation(const glm::quat& rotation);
	glm::quat GetRotation() const;

	Entity* SetScale(const glm::vec3& scale);
	glm::vec3 GetScale() const;

	Entity* SetAngle(const glm::vec3& angle);
	glm::vec3 GetAngles() const;
protected:
	glm::vec3 m_position = {};
	glm::quat m_rotation = {};
	glm::vec3 m_angles = {};
	glm::vec3 m_scale = {};
};

#define RAW_DATA(data) glm::value_ptr(data)