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

	CreateRenderPass();
	CreateDescriptorPool();
	CreateFrameBuffers();

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
		.RenderPass = m_renderPass,
		.MinImageCount = m_graphics->m_windowSurfaceCapabilities.minImageCount,
		.ImageCount = m_graphics->m_swapChainImageCount,
		.MSAASamples = static_cast<VkSampleCountFlagBits>(m_graphics->m_MSAALevel),
		.PipelineCache = 0,
		.Subpass = 0,
		.DescriptorPoolSize = 0,
		.UseDynamicRendering = false,
		.PipelineRenderingCreateInfo = {},
		.Allocator = 0,
		.CheckVkResultFn = CheckVKResult
	};
	ImGui_ImplVulkan_Init(&initInfo);

	m_commandBuffers = m_graphics->CreateCommandBuffers(m_graphics->m_swapChainImageCount);
}

IMGUIManager::~IMGUIManager()
{

}

void IMGUIManager::CreateRenderPass()
{
	vk::AttachmentDescription colorAttachment = {
		/* flags */         {},
		/* format */        m_graphics->m_swapChainInfo.imageFormat,
		/* samples */       m_graphics->m_MSAALevel,
		/* loadOp */        vk::AttachmentLoadOp::eLoad,
		/* storeOp */       vk::AttachmentStoreOp::eStore,
		/* stencilLoadOp */ vk::AttachmentLoadOp::eDontCare,
		/* stencilStoreOp*/ vk::AttachmentStoreOp::eDontCare,
		/* initialLayout */ vk::ImageLayout::eColorAttachmentOptimal,
		/* finalLayout */   vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::AttachmentReference colorAttachmentRef = {
		/* attachment */ 0,
		/* layout */     vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::AttachmentDescription colorAttachmentResolve = {
		/* flags */         {},
		/* format */        m_graphics->m_swapChainInfo.imageFormat,
		/* samples */       vk::SampleCountFlagBits::e1,
		/* loadOp */        vk::AttachmentLoadOp::eDontCare,
		/* storeOp */       vk::AttachmentStoreOp::eStore,
		/* stencilLoadOp */ vk::AttachmentLoadOp::eDontCare,
		/* stencilStoreOp*/ vk::AttachmentStoreOp::eDontCare,
		/* initialLayout */ vk::ImageLayout::eUndefined,
		/* finalLayout */   vk::ImageLayout::ePresentSrcKHR
	};
	vk::AttachmentReference colorAttachmentResolveRef = {
		/* attachment */ 1,
		/* layout */     vk::ImageLayout::eColorAttachmentOptimal
	};

	std::vector<vk::AttachmentDescription> attachments = { colorAttachment, colorAttachmentResolve };

	vk::SubpassDescription subpass = {
		.flags = {},
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = &colorAttachmentResolveRef,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr
	};

	vk::SubpassDependency dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
	};

	vk::RenderPassCreateInfo renderPassInfo = {
		.flags = {},
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.pDependencies = &dependency
	};

	if (m_graphics->m_logicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_renderPass) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create render pass for IMGUI!");
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

void IMGUIManager::CreateFrameBuffers()
{
	m_frameBuffers = std::vector<vk::Framebuffer>(m_graphics->m_swapChainImageCount);

	for (auto i = 0; i < m_frameBuffers.size(); i++)
	{
		std::vector<vk::ImageView> attachments = { m_graphics->m_MSAAImageView, m_graphics->m_swapChainImageViews[i] };

		vk::FramebufferCreateInfo info = {
			.flags = {},
			.renderPass = m_renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = m_graphics->m_swapChainInfo.imageExtent.width,
			.height = m_graphics->m_swapChainInfo.imageExtent.height,
			.layers = 1
		};

		if (m_graphics->m_logicalDevice.createFramebuffer(&info, nullptr, &m_frameBuffers[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create framebuffer!");
	}
}

void IMGUIManager::RenderFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	WindowSetup();

	ImGui::Render();
}

void IMGUIManager::WindowSetup()
{
	ImGui::ShowDemoWindow();
}

vk::CommandBuffer IMGUIManager::RecordCommandBuffer(const uint32_t& imageIndex)
{
	auto commandBuffer = m_commandBuffers[imageIndex];
	m_graphics->BeginCommandBuffer(commandBuffer, vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	vk::ClearValue clearColor = {
		.color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }
	};
	vk::RenderPassBeginInfo renderPassInfo = {
		.renderPass = m_renderPass,
		.framebuffer = m_frameBuffers[imageIndex],
		.renderArea = vk::Rect2D { {0, 0}, m_graphics->m_swapChainInfo.imageExtent },
		.clearValueCount = 1,
		.pClearValues = &clearColor,
	};
	commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	commandBuffer.endRenderPass();
	commandBuffer.end();

	return commandBuffer;
}

void IMGUIManager::Cleanup()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	for (auto& frameBuffer : m_frameBuffers)
		m_graphics->m_logicalDevice.destroyFramebuffer(frameBuffer, nullptr);
		
	m_graphics->m_logicalDevice.destroyDescriptorPool(m_descriptorPool, nullptr);
	m_graphics->m_logicalDevice.destroyRenderPass(m_renderPass, nullptr);
}