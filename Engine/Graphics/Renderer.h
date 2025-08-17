#pragma once

#include "../Objects/ObjectManager.h"
#include "Models/TextureManager.h"
#include "Models/ModelManager.h"
#include "../Objects/CameraManager.h"
#include "IMGUI/IMGUIManager.h"
#include "../Utility/PerformanceManager.h"

class Renderer
{
	CLASS_SINGLETON_HEADER(Renderer)
public:
	vk::CommandBuffer RecordCommandBuffer(const uint32_t& imageIndex);
	void RenderFrame();
	Renderer();
	~Renderer();
private:
	std::shared_ptr<Graphics>					m_graphics = nullptr;
	std::shared_ptr<ModelManager>				m_modelManger = nullptr;
	std::shared_ptr<TextureManager>				m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>				m_objectManager = nullptr;
	std::shared_ptr<CameraManager>				m_cameraManager = nullptr;
	std::shared_ptr<IMGUIManager>				m_imguiManager = nullptr;
	std::shared_ptr<PerformanceManager>			m_performanceManager = nullptr;
};
