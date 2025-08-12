#pragma once

#include "../Graphics.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

class IMGUIManager
{
	static std::shared_ptr<IMGUIManager> s_instance;
public:
	static std::shared_ptr<IMGUIManager> Get();
	static void CheckVKResult(VkResult err);
	IMGUIManager();
	~IMGUIManager();
	void CreateRenderPass();
	void CreateDescriptorPool();
	void CreateFrameBuffers();
	void RenderFrame();
	void WindowSetup();
	vk::CommandBuffer RecordCommandBuffer(const uint32_t& imageIndex);
	void Cleanup();
private:
	std::shared_ptr<Graphics>			m_graphics = nullptr;

	std::vector<vk::CommandBuffer>		m_commandBuffers = {};
	vk::DescriptorPool					m_descriptorPool = {};
	vk::RenderPass						m_renderPass = {};
	std::vector<vk::Framebuffer>		m_frameBuffers = {};
};

