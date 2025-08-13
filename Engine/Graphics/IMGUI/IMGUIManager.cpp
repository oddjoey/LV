#include "IMGUIManager.h"

std::shared_ptr<IMGUIManager> IMGUIManager::s_instance = nullptr;

std::shared_ptr<IMGUIManager> IMGUIManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<IMGUIManager>(new IMGUIManager());

	return s_instance;
}

void IMGUIManager::CheckVKResult(VkResult err)
{
	if (err == VK_SUCCESS)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

IMGUIManager::IMGUIManager()
{
	m_graphics = Graphics::Get();
	CreateDescriptorPool();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableSetMousePos;
	io.DisplaySize = ImVec2 { static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.width),
		static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.height) };

	ImGui::StyleColorsDark();

	if (!ImGui_ImplGlfw_InitForVulkan(m_graphics->m_window, true))
		throw std::runtime_error("ImGui_ImplGlfw_InitForVulkan failed!");

	ImGui_ImplVulkan_InitInfo initInfo = {
		.ApiVersion = m_graphics->applicationInfo.apiVersion,
		.Instance = m_graphics->m_vulkanInstance,
		.PhysicalDevice = m_graphics->m_physicalDevice,
		.Device = m_graphics->m_logicalDevice,
		.QueueFamily = m_graphics->m_physicalDeviceGraphicsIndex,
		.Queue = m_graphics->m_queueGraphics,
		.DescriptorPool = m_descriptorPool,
		.RenderPass = m_graphics->m_renderPass,
		.MinImageCount = m_graphics->m_windowSurfaceCapabilities.minImageCount,
		.ImageCount = m_graphics->m_swapChainImageCount,
		.MSAASamples = static_cast<VkSampleCountFlagBits>(m_graphics->m_MSAALevel),
		.PipelineCache = 0,
		.Subpass = 1,
		.DescriptorPoolSize = 0,
		.UseDynamicRendering = false,
		.PipelineRenderingCreateInfo = {},
		.Allocator = 0,
		.CheckVkResultFn = CheckVKResult
	};
	ImGui_ImplVulkan_Init(&initInfo);
}

IMGUIManager::~IMGUIManager()
{

}

void IMGUIManager::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		//{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE }
		//{ vk::DescriptorType::eSampledImage, 1000 },
		//{ vk::DescriptorType::eStorageImage, 1000 },
		//{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		//{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		//{ vk::DescriptorType::eUniformBuffer, 1000 },
		//{ vk::DescriptorType::eStorageBuffer, 1000 },
		//{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		//{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		//{ vk::DescriptorType::eInputAttachment, 1000 }
	};
	
	vk::DescriptorPoolCreateInfo poolCreateInfo = {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = static_cast<uint32_t>(IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (m_graphics->m_logicalDevice.createDescriptorPool(&poolCreateInfo, 0, &m_descriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create IMGUI descriptor pool!");
}

void IMGUIManager::WindowSetup()
{
	ImGui::ShowDemoWindow();
}

void IMGUIManager::RecordCommandBuffer(const vk::CommandBuffer& commandBuffer)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	WindowSetup();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void IMGUIManager::Cleanup()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
		
	m_graphics->m_logicalDevice.destroyDescriptorPool(m_descriptorPool, nullptr);
}