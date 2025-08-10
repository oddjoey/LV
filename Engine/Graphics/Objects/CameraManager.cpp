#include "CameraManager.h"

std::shared_ptr<CameraManager> CameraManager::s_instance = nullptr;

std::shared_ptr<CameraManager> CameraManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<CameraManager>(new CameraManager());

	return s_instance;
}

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
	camera->m_index = static_cast<uint32_t>(m_cameras.size());
	m_cameras.push_back(camera);
}

pCamera CameraManager::GetActiveCamera() const
{
	return m_cameras[m_activeCameraIndex];
}

void CameraManager::SetActiveCamera(const pCamera& camera)
{
	m_activeCameraIndex = camera->GetIndex();
}

std::vector<pCamera> CameraManager::GetCameras() const
{
	return m_cameras;
}
