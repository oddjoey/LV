#include "CameraManager.h"

CLASS_SINGLETON_SOURCE(CameraManager)

CameraManager::CameraManager()
{

}

CameraManager::~CameraManager()
{

}

pCamera CameraManager::CreateCamera(const Camera& cameraInfo)
{
	auto camera = std::make_shared<Camera>(cameraInfo);
	AddCamera(camera);
	return camera;
}

void CameraManager::AddCamera(const pCamera& camera)
{
	m_cameras.push_back(camera);
	camera->m_index = static_cast<uint32_t>(m_cameras.size());
}

void CameraManager::RemoveCamera(const pCamera& camera)
{
	if (GetActiveCamera() == camera)
		SetActiveCamera(nullptr);

	m_cameras.erase(std::remove(m_cameras.begin(), m_cameras.end(), camera), m_cameras.end());

	for (uint32_t i = 0; i < m_cameras.size(); i++)
		m_cameras[i]->m_index = i + 1;
}

void CameraManager::RemoveCamera(const uint32_t& index)
{
	if (!index)
		return;

	RemoveCamera(m_cameras[index - 1]);
}

pCamera CameraManager::GetActiveCamera() const
{
	if (!m_activeCameraIndex)
		return nullptr;

	return m_cameras[m_activeCameraIndex - 1];
}

void CameraManager::SetActiveCamera(const pCamera& camera)
{
	if (camera == nullptr)
	{
		m_activeCameraIndex = 0;
		return;
	}

	m_activeCameraIndex = camera->GetIndex();
}

std::vector<pCamera> CameraManager::GetCameras() const
{
	return m_cameras;
}
