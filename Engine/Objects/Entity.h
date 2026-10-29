#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

	// Translation * rotation * scale. Kept from one call to the next and only worked out again when one
	// of them has changed: most things don't move from frame to frame.
	const glm::mat4& GetModelMatrix() const;
protected:
	glm::vec3 m_position = {};
	glm::quat m_rotation = {};
	glm::vec3 m_angles = {};
	glm::vec3 m_scale = {};
private:
	mutable glm::mat4 m_matrix = glm::mat4(1.f);
	mutable glm::vec3 m_matrixPosition = {};
	mutable glm::quat m_matrixRotation = {};
	mutable glm::vec3 m_matrixScale = {};
	mutable bool m_matrixValid = false;
};

#define RAW_DATA(data) glm::value_ptr(data)