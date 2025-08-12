#pragma once

#include <vector>

#include "Camera.h"

class CameraManager
{
public:
	CameraManager();
	~CameraManager();

	static std::shared_ptr<CameraManager> Get();

	pCamera CreateCamera(const Camera& cameraInfo);
	void AddCamera(const pCamera& camera);

	pCamera GetActiveCamera() const;
	void SetActiveCamera(const pCamera& camera);

	std::vector<pCamera> GetCameras() const;
private:

	static std::shared_ptr<CameraManager> s_instance;

	std::vector<pCamera> m_cameras = {};
	uint32_t m_activeCameraIndex = 0;
};