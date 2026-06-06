#pragma once

#include "../Graphics.h"
#include "../../Objects/CameraManager.h"
#include "../../Objects/ObjectManager.h"
#include "../Models/ModelManager.h"
#include "../Models/TextureManager.h"
#include "../../Utility/Utility.h"
#include "../../Utility/PerformanceManager.h"

#include <imgui.h>
#include <../../../Libraries/imgui/backends/imgui_impl_glfw.h>
#include <../../../Libraries/imgui/backends/imgui_impl_vulkan.h>

class IMGUIManager
{
	friend class InputManager;
	friend class Renderer;
	CLASS_SINGLETON_HEADER(IMGUIManager)
public:
	IMGUIManager();
	~IMGUIManager();

	void WindowSetup();

	void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer);

	void Cleanup();
private:
	static void CheckVKResult(VkResult err);
	void CreateDescriptorPool();
	vk::DescriptorPool						m_descriptorPool = {};
	bool									m_showIMGUI = true;

	std::shared_ptr<Graphics>				m_graphics = nullptr;
	std::shared_ptr<CameraManager>			m_cameraManager = nullptr;
	std::shared_ptr<ObjectManager>			m_objectManager = nullptr;
	std::shared_ptr<ModelManager>			m_modelManager = nullptr;
	std::shared_ptr<TextureManager>			m_textureManager = nullptr;
	std::shared_ptr<PerformanceManager>		m_performanceManager = nullptr;
};