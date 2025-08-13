#pragma once

#include "../Graphics.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

class IMGUIManager
{
	static std::shared_ptr<IMGUIManager> s_instance;
	friend class InputManager;
	friend class Renderer;
public:
	static std::shared_ptr<IMGUIManager> Get();
	IMGUIManager();
	~IMGUIManager();
	void WindowSetup();
	void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer);
	void Cleanup();
private:
	static void CheckVKResult(VkResult err);
	void CreateDescriptorPool();
	std::shared_ptr<Graphics>			m_graphics = nullptr;
	vk::DescriptorPool					m_descriptorPool = {};
	bool								m_showIMGUI = false;
};