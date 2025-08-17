#pragma once

#include <memory>

#include "Entity.h"

class Camera : public Entity
{
	friend class CameraManager;
	friend class IMGUIManager;
public:
	Camera(const glm::vec3& position = {}, const glm::quat& rotation = {}, const glm::vec3& scale = { 1.f, 1.f, 1.f }, const float& fov = 45.f, const float& nearPlane = 0.1f, const float& farPlane = 10.f);
	~Camera();

	Camera* LookAt(const glm::vec3& position);

	glm::mat4 CalculateViewMatrix();
	Camera* SetViewMatrix(const glm::mat4& viewMatrix);
	glm::mat4 GetViewMatrix() const;

	uint32_t GetIndex() const;

	Camera* SetFov(const float& fov);
	float GetFov() const;

	Camera* SetNearPlane(const float& nearPlane);
	float GetNearPlane() const;

	Camera* SetFarPlane(const float& farPlane);
	float GetFarPlane() const;
private:
	glm::mat4 m_viewMatrix;
	uint32_t m_index;
	float m_fov;
	float m_nearPlane;
	float m_farPlane;
};

typedef std::shared_ptr<Camera> pCamera;