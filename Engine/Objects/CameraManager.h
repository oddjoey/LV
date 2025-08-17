#pragma once

#include <vector>

#include "Camera.h"
#include "../Utility/Utility.h"

class CameraManager
{
	CLASS_SINGLETON_HEADER(CameraManager)
public:
	CameraManager();
	~CameraManager();

	pCamera CreateCamera(const Camera& cameraInfo);
	void AddCamera(const pCamera& camera = nullptr);

	void RemoveCamera(const pCamera& camera);
	void RemoveCamera(const uint32_t& index);

	pCamera GetActiveCamera() const;
	void SetActiveCamera(const pCamera& camera);

	std::vector<pCamera> GetCameras() const;
private:
	std::vector<pCamera> m_cameras = {};
	uint32_t m_activeCameraIndex = 0;
};