#pragma once

#include <memory>

#include "Entity.h"

class Camera : public Entity
{
	friend class CameraManager;
public:
	Camera(const glm::vec3& position = {}, const glm::quat& rotation = {}, const glm::vec3& scale = {}, const float& fov = 0, const float& nearPlane = 0, const float& farPlane = 0);
	~Camera();

	void LookAt(const glm::vec3& position);

	void SetViewMatrix(const glm::mat4& viewMatrix);
	glm::mat4 GetViewMatrix() const;

	void SetRotation(const glm::quat& rotation) override;

	uint32_t GetIndex() const;

	void SetFov(const float& fov);
	float GetFov() const;

	void SetNearPlane(const float& nearPlane);
	float GetNearPlane() const;

	void SetFarPlane(const float& farPlane);
	float GetFarPlane() const;
private:
	glm::mat4 m_viewMatrix;
	uint32_t m_index;
	float m_fov;
	float m_nearPlane;
	float m_farPlane;
};

typedef std::shared_ptr<Camera> pCamera;