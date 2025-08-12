#pragma once

#include "../Objects/ObjectManager.h"
#include "Models/TextureManager.h"
#include "Models/ModelManager.h"
#include "../Objects/CameraManager.h"
#include "IMGUI/IMGUIManager.h"

class Renderer
{
	static std::shared_ptr<Renderer> _instance;
public:
	static std::shared_ptr<Renderer> Get();
	vk::CommandBuffer RecordCommandBuffer(const uint32_t& imageIndex);
	void RenderFrame();
	Renderer();
	~Renderer();
private:
	std::shared_ptr<Graphics>			m_graphics = nullptr;
	std::shared_ptr<ModelManager>		m_modelManger = nullptr;
	std::shared_ptr<TextureManager>		m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>		m_objectManager = nullptr;
	std::shared_ptr<CameraManager>		m_cameraManager = nullptr;
	std::shared_ptr<IMGUIManager>		m_imguiManager = nullptr;
};
