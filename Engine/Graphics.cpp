#include "Graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

std::shared_ptr<Graphics> Graphics::s_pInstance = nullptr;

std::shared_ptr<Graphics> Graphics::Get()
{
	if (s_pInstance == nullptr)
		s_pInstance = std::shared_ptr<Graphics>(new Graphics());

	return s_pInstance;
}

Graphics::Graphics()
{

}

Graphics::~Graphics()
{
	CleanupVulkan();
}

void Graphics::Initialize()
{
	LOG_INFO("Initializing Graphics...");

	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	InitializeWindow();

	GetVulkanData();

	CreateVulkanInstance();

#ifdef _DEBUG
	if (IsDebugSupported())
		InitializeDebugger();
	else
		throw std::runtime_error("required layers for debugging not found.");
#endif
	CreateWindowSurface();

	PickPhysicalDevice();
	CreateLogicalDevice();

	CreateSwapChain();
	CreateImageViews();

	CreateRenderPass();

	CreateDescriptorSetLayout();

	CreateGraphicsPipeline();

	CreateFrameBuffers();

	CreateCommandPools();

	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();

	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();

	CreateDescriptorPool();
	CreateDescriptorSets();

	CreateCommandBuffers();

	CreateSyncObjects();

	LOG_SUCCESS("Graphics initialized successfully!");
}

void Graphics::CreateVulkanInstance() {
	LOG_INFO("Creating Vulkan instance...");

	vk::ApplicationInfo appInfo { 
		.pApplicationName = "Lordi",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Lordi Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	vk::InstanceCreateInfo createInfo {
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = 0,
	};

	uint32_t requiredGLFWExtensions = 0;
	const char** ppExtensions = glfwGetRequiredInstanceExtensions(&requiredGLFWExtensions);
	if (!ppExtensions)
		throw std::runtime_error("Required GLFW extensions not found!");

	std::vector<const char*> requiredExtensionNames(ppExtensions, ppExtensions + requiredGLFWExtensions);

#ifdef _DEBUG
	requiredExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	createInfo.enabledLayerCount = static_cast<uint32_t>(m_vRequiredValidationLayerNames.size());
	createInfo.ppEnabledLayerNames = m_vRequiredValidationLayerNames.data();

	m_sDebugCreateInfo.sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT;
	m_sDebugCreateInfo.messageSeverity = 
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	m_sDebugCreateInfo.messageType = 
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	m_sDebugCreateInfo.pfnUserCallback = DebugCallback;

	createInfo.pNext = &m_sDebugCreateInfo;
#endif // _DEBUG

	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size());
	createInfo.ppEnabledExtensionNames = requiredExtensionNames.data();

	if (vk::createInstance(&createInfo, nullptr, &m_pVulkanInstance) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create vulkan instance");

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_pVulkanInstance);

	LOG_SUCCESS("Vulkan instance created successfully!");
}

void Graphics::GetVulkanData()
{
	LOG_INFO("Getting Vulkan data...");
	
	uint32_t instanceExtensionCount = 0;
	
	if (vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr) != vk::Result::eSuccess || !instanceExtensionCount)
		throw std::runtime_error("enumerateInstanceExtensionProperties failed!");

	m_vVulkanExtensionProps = std::vector<vk::ExtensionProperties>(instanceExtensionCount);
	if (vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, m_vVulkanExtensionProps.data()) != vk::Result::eSuccess || !instanceExtensionCount)
		throw std::runtime_error("enumerateInstanceExtensionProperties failed!");

	uint32_t instanceLayerCount = 0;
	if (vk::enumerateInstanceLayerProperties(&instanceLayerCount, nullptr) != vk::Result::eSuccess || !instanceLayerCount)
		throw std::runtime_error("enumerateInstanceLayerProperties failed!");

	m_vVulkanLayerProps = std::vector<vk::LayerProperties>(instanceLayerCount);
	if (vk::enumerateInstanceLayerProperties(&instanceLayerCount, m_vVulkanLayerProps.data()) != vk::Result::eSuccess || !instanceLayerCount)
		throw std::runtime_error("enumerateInstanceLayerProperties failed!");

	LOG_SUCCESS("Vulkan data retrieved successfully!");
}

void Graphics::PickPhysicalDevice()
{
	LOG_INFO("Picking physical device (GPU)...");

	unsigned int deviceCount = 0;
	;
	if (m_pVulkanInstance.enumeratePhysicalDevices(&deviceCount, nullptr) != vk::Result::eSuccess || deviceCount == 0)
		throw std::runtime_error("No device found with Vulkan support");

	auto devices = std::vector<vk::PhysicalDevice>(deviceCount);
	if (m_pVulkanInstance.enumeratePhysicalDevices(&deviceCount, devices.data()) != vk::Result::eSuccess || deviceCount == 0)
		throw std::runtime_error("No device found with Vulkan support");

	for (const auto& device : devices)
		if (IsDeviceSuitable(device))
		{
			m_pPhysicalDevice = device;
			break;
		}

	if (m_pPhysicalDevice == nullptr)
		throw std::runtime_error("Failed to find a suitable device");

	LOG_SUCCESS("Physical device (GPU) picked successfully!");
}

void Graphics::CreateLogicalDevice()
{
	LOG_INFO("Creating logical device...");
	std::vector<vk::DeviceQueueCreateInfo> deviceQueuesInfo;
	std::set<uint32_t> uniqueQueueFamilies { m_uiDeviceGraphicsIndex, m_uiDevicePresentIndex, m_uiDeviceTransferIndex };

	float queuePriority = 1.0f; // higher values indicate higher priority
	for (auto queueFamily : uniqueQueueFamilies)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo {
			.queueFamilyIndex = queueFamily,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		deviceQueuesInfo.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures {};
	deviceFeatures.samplerAnisotropy = vk::True;

	vk::DeviceCreateInfo deviceCreateInfo {
		.flags = {},
		.queueCreateInfoCount = static_cast<uint32_t>(deviceQueuesInfo.size()),
		.pQueueCreateInfos = deviceQueuesInfo.data(),
		.enabledExtensionCount = static_cast<uint32_t>(m_vRequiredDeviceExtensionNames.size()),
		.ppEnabledExtensionNames = m_vRequiredDeviceExtensionNames.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

//#ifdef _DEBUG
//	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_cvRequiredValidationLayerNames.size());
//	deviceCreateInfo.ppEnabledLayerNames = m_cvRequiredValidationLayerNames.data();
//#endif // _DEBUG

	if (m_pPhysicalDevice.createDevice(&deviceCreateInfo, nullptr, &m_pLogicalDevice) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create logical device!");
	
	m_pLogicalDevice.getQueue(m_uiDeviceGraphicsIndex, 0, &m_pGraphicsQueue);
	m_pLogicalDevice.getQueue(m_uiDevicePresentIndex, 0, &m_pPresentQueue);
	m_pLogicalDevice.getQueue(m_uiDeviceTransferIndex, 0, &m_pTransferQueue);

	LOG_SUCCESS("Logical device created successfully!");
}

void Graphics::CreateSwapChain()
{
	LOG_INFO("Creating swap chain...");

	m_sSwapChainFormat = m_vSwapChainFormats[0];
	bool formatFound = false;
	for (auto& format : m_vSwapChainFormats)
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			formatFound = true;
			m_sSwapChainFormat = format;
			break;
		}
	if (!formatFound)
		LOG_WARNING("Requested swapchain format/color space not found on device, using device's first format/color format...");

	auto graphicsSettings = Settings::Get()->GetGraphicsSettings();
	switch (graphicsSettings.eVSync)
	{
		case VSyncOptions::NONE:
			m_eUserPresentMode = vk::PresentModeKHR::eImmediate; // No VSync
			LOG_DEBUG("VSyncOptions::NONE / VK_PRESENT_MODE_IMMEDIATE_KHR");
			break;
		case VSyncOptions::ENABLED:
			m_eUserPresentMode = vk::PresentModeKHR::eFifo; // VSync
			LOG_DEBUG("VSyncOptions::ENABLED / VK_PRESENT_MODE_FIFO_KHR");
			break;
		case VSyncOptions::RELAXED:
			m_eUserPresentMode = vk::PresentModeKHR::eFifoRelaxed; // Relaxed VSync
			LOG_DEBUG("VSyncOptions::RELAXED / VK_PRESENT_MODE_FIFO_RELAXED_KHR");
			break;
		case VSyncOptions::MAILBOX:
			m_eUserPresentMode = vk::PresentModeKHR::eMailbox; // Mailbox VSync
			LOG_DEBUG("VSyncOptions::MAILBOX / VK_PRESENT_MODE_MAILBOX_KHR");
			break;
	}

	vk::PresentModeKHR swapChainPresentMode = vk::PresentModeKHR::eFifo;
	bool presentFound = false;
	for (const auto& presentMode : m_vSwapChainPresentModes)
		if (presentMode == m_eUserPresentMode)
		{
			presentFound = true;
			swapChainPresentMode = presentMode;
			break;
		}
	if (!presentFound)
		LOG_WARNING("Requested present mode not found on device, using defaults (FIFO / VYSNC) as its guaranteed...");

	vk::Extent2D swapChainExtent = {};
	if (m_sSwapChainCapabilities.currentExtent.width == UINT32_MAX)
	{
		int width, height;
		glfwGetFramebufferSize(m_pWindow, &width, &height);
		swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), m_sSwapChainCapabilities.minImageExtent.width, m_sSwapChainCapabilities.maxImageExtent.width);
		swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), m_sSwapChainCapabilities.minImageExtent.height, m_sSwapChainCapabilities.maxImageExtent.height);
	}
	else
		swapChainExtent = m_sSwapChainCapabilities.currentExtent;

	uint32_t swapChainImageCount = m_sSwapChainCapabilities.minImageCount + 1;
	if (m_sSwapChainCapabilities.maxImageCount > 0 && swapChainImageCount > m_sSwapChainCapabilities.maxImageCount)
	{
		LOG_WARNING("Swap chain image count exceeds maximum, clamping to maximum...");
		swapChainImageCount = m_sSwapChainCapabilities.maxImageCount;
	}

	m_sSwapChainInfo = {
		.flags = {},
		.surface = m_pSurface,
		.minImageCount = swapChainImageCount,
		.imageFormat = m_sSwapChainFormat.format,
		.imageColorSpace = m_sSwapChainFormat.colorSpace,
		.imageExtent = swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment
	};

	if (m_uiDeviceGraphicsIndex != m_uiDevicePresentIndex || m_uiDeviceGraphicsIndex != m_uiDeviceTransferIndex)
	{
		LOG_WARNING("Needed features use separate queue families, using concurrent mode for swap chain images.");
		// If the graphics queue family is different from the present queue family, we need to specify the queue families
		std::set<uint32_t> uniqueQueueFamilyIndices = { m_uiDeviceGraphicsIndex, m_uiDevicePresentIndex, m_uiDeviceTransferIndex };
		std::vector<uint32_t> queueFamilyIndices(uniqueQueueFamilyIndices.size());

		for (auto& index : uniqueQueueFamilyIndices)
			queueFamilyIndices.push_back(index);

		m_sSwapChainInfo.imageSharingMode = vk::SharingMode::eConcurrent; // images can be shared between multiple queues
		m_sSwapChainInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
		m_sSwapChainInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	}
	else
	{
		LOG_INFO("Required features use the same queue family, using exclusive mode for swap chain images.");
		// If the graphics queue family is the same as the present queue family, we can use exclusive mode
		m_sSwapChainInfo.imageSharingMode = vk::SharingMode::eExclusive;
		m_sSwapChainInfo.queueFamilyIndexCount = 0;
		m_sSwapChainInfo.pQueueFamilyIndices = nullptr;
	}

	m_sSwapChainInfo.preTransform = m_sSwapChainCapabilities.currentTransform;
	m_sSwapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	// presentation mode (VSync)
	m_sSwapChainInfo.presentMode = swapChainPresentMode;
	// clipped swap chain images provide the best performance compared to non-clipped images
	// clipped images are not presented if they are obscured by other windows
	m_sSwapChainInfo.clipped = VK_TRUE;
	// if recreating swapchain, provide previous one here
	// TO-DO: Implement swap chain recreation logic
	m_sSwapChainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (m_pLogicalDevice.createSwapchainKHR(&m_sSwapChainInfo, nullptr, &m_pSwapChain) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create swapchain!");

	// Get swap chain images
	if (m_pLogicalDevice.getSwapchainImagesKHR(m_pSwapChain, &swapChainImageCount, nullptr) != vk::Result::eSuccess || !swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	m_vSwapChainImages = std::vector<vk::Image>(swapChainImageCount);
	if (m_pLogicalDevice.getSwapchainImagesKHR(m_pSwapChain, &swapChainImageCount, m_vSwapChainImages.data()) != vk::Result::eSuccess || !swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	LOG_SUCCESS("Swapchain created successfully!");
}

void Graphics::CreateImageViews()
{
	LOG_INFO("Creating image views for swapchain images...");

	m_vSwapChainImageViews = std::vector<vk::ImageView>(m_vSwapChainImages.size());
	for (auto i = 0; i < m_vSwapChainImages.size(); i++)
		m_vSwapChainImageViews[i] = CreateImageView(m_vSwapChainImages[i], m_sSwapChainFormat.format);

	LOG_SUCCESS("Image views for swapchain images created successfully!");
}

void Graphics::CreateRenderPass()
{
	LOG_INFO("Creating render pass...");

	LOG_DEBUG("Setting multisampling for color attachment based on user's settings:");
	auto settings = Settings::Get()->GetGraphicsSettings();
	switch (settings.eMSAA)
	{
	case MSAAOptions::X1:
		m_eUserMSAA = vk::SampleCountFlagBits::e1; // no multisampling
		LOG_DEBUG("MSAAOptions::X1 / VK_SAMPLE_COUNT_1_BIT");
		break;
	case MSAAOptions::X2:
		m_eUserMSAA = vk::SampleCountFlagBits::e2; // 2x multisampling
		LOG_DEBUG("MSAAOptions::X2 / VK_SAMPLE_COUNT_2_BIT");
		break;
	case MSAAOptions::X4:
		m_eUserMSAA = vk::SampleCountFlagBits::e4; // 4x multisampling
		LOG_DEBUG("MSAAOptions::X4 / VK_SAMPLE_COUNT_4_BIT");
		break;
	case MSAAOptions::X8:
		m_eUserMSAA = vk::SampleCountFlagBits::e8; // 8x multisampling
		LOG_DEBUG("MSAAOptions::X8 / VK_SAMPLE_COUNT_8_BIT");
		break;
	case MSAAOptions::X16:
		m_eUserMSAA = vk::SampleCountFlagBits::e16; // 16x multisampling
		LOG_DEBUG("MSAAOptions::X16 / VK_SAMPLE_COUNT_16_BIT");
		break;
	case MSAAOptions::X32:
		m_eUserMSAA = vk::SampleCountFlagBits::e32; // 32x multisampling
		LOG_DEBUG("MSAAOptions::X32 / VK_SAMPLE_COUNT_32_BIT");
		break;
	case MSAAOptions::X64:
		m_eUserMSAA = vk::SampleCountFlagBits::e64; // 64x multisampling
		LOG_DEBUG("MSAAOptions::X64 / VK_SAMPLE_COUNT_64_BIT");
		break;
	}

	LOG_DEBUG("Creating render pass attachment description:");
	// Render pass is a collection of attachments (metadata about images) and subpasses (rendering operations)
	// Attachments defines how images are used during rendering
	// each attachment adds data to each pixel, depending on attachment type
	vk::AttachmentDescription colorAttachmentDesc {
		.flags = {},
		.format = m_sSwapChainInfo.imageFormat,
		.samples = m_eUserMSAA,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR
	};
	// value specifying how the contents of color and depth components of the attachment are treated at the beginning of the subpass where it is first used.
	// VK_ATTACHMENT_LOAD_OP_LOAD (preserves data)
	// VK_ATTACHMENT_LOAD_OP_CLEAR (clears data)
	// VK_ATTACHMENT_LOAD_OP_DONT_CARE (ignores the data)
	// value specifying how the contents of color and depth components of the attachment are treated at the end of the subpass where it is last used.
	// VK_ATTACHMENT_STORE_OP_STORE stores the data in the attachment in memory to be used later
	// VK_ATTACHMENT_STORE_OP_DONT_CARE ignores the data in the attachment, data is not preserved
	// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
	//  -   images used as color attachments during rendering
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//  -   images used for presentation to the screen 
	// VK_IMAGE_LAYOUT_UNDEFINED 
	//  -   dont care about initial layout, we'll set it in finalLayout, also data not guaranteed to be preserved
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
	//  -   images used for transfer operations (e.g. blitting, copying, etc.)
	// Attachment reference is used to specify how the attachment is used in the subpass
	vk::AttachmentReference colorAttachmentRef {
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};

	// A single render pass can have multiple subpasses,
	// subpasses are subsequent rendering operations that depend on the contents
	// of the previous framebuffers from previous passes. e.g. post-processing effects
	// other types of attachments:
	// pInputAttachments - input attachments (used as input to the subpass)
	// pResolveAttachments - resolve attachments (used for multisampling)
	// pDepthStencilAttachment - depth/stencil attachments (used for depth/stencil testing)
	// pPreserveAttachments - preserve attachments (used to preserve the contents of the attachment)
	vk::SubpassDescription subpassDesc { 
		.flags = {},
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	vk::SubpassDependency subpassDependency {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
	};

	vk::RenderPassCreateInfo renderPassInfo {
		.flags = {},
		.attachmentCount = 1,
		.pAttachments = &colorAttachmentDesc,
		.subpassCount = 1,
		.pSubpasses = &subpassDesc,
		.dependencyCount = 1,
		.pDependencies = &subpassDependency
	};

	if (m_pLogicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_pRenderPass) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create render pass!");

	LOG_SUCCESS("Render pass created successfully!");
}

void Graphics::CreateGraphicsPipeline()
{
	LOG_INFO("Creating graphics pipeline...");

	LOG_DEBUG("Creating shader modules for vertex and fragment shaders...");

	auto graphicsSettings = Settings::Get()->GetGraphicsSettings();

	// Load shader byte code into memory
	auto vertShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/vert.spv");
	vk::ShaderModuleCreateInfo vertShaderInfo{
		.flags = {},
		.codeSize = vertShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data())
	};
	if (m_pLogicalDevice.createShaderModule(&vertShaderInfo, nullptr, &m_pVertexShaderModule) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create vertex shader module!");
	auto fragShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/frag.spv");
	vk::ShaderModuleCreateInfo fragShaderInfo {
		.flags = {},
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data())
	};
	if (m_pLogicalDevice.createShaderModule(&fragShaderInfo, nullptr, &m_pFragmentShaderModule) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create fragment shader module!");

	// Shader code
	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.flags = {},
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = m_pVertexShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr
	};
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
		.flags = {},
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = m_pFragmentShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr
	};
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{
		vertShaderStageInfo,
		fragShaderStageInfo
	};

	// Vertex data
	vk::VertexInputBindingDescription vertexBindingDesc {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex
	};
	std::vector<vk::VertexInputBindingDescription> vertexBindingDescs {
		vertexBindingDesc
	};
	vk::VertexInputAttributeDescription vertexPositionDesc {
		.location = 0,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, position)
	};
	vk::VertexInputAttributeDescription vertexColorDesc {
		.location = 1,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, color)
	};
	vk::VertexInputAttributeDescription vertexTexDesc {
		.location = 2,
		.binding = 0,
		.format = vk::Format::eR32G32Sfloat,
		.offset = offsetof(Vertex, texCoord)
	};
	std::vector<vk::VertexInputAttributeDescription> vertexDescriptions {
		vertexPositionDesc,
		vertexColorDesc,
		vertexTexDesc
	};
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
		.flags = {},
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBindingDesc,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescriptions.size()),
		.pVertexAttributeDescriptions = vertexDescriptions.data()
	};

	// How primitives are created in vertex shader
	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo {
		.flags = {},
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = VK_FALSE
	};

	// Increases primitive geometry for smoother surfaces
	//vk::PipelineTessellationStateCreateInfo tessellationInfo {};

	// Viewport (scale image to size), Scissor (crop image to size)
	vk::PipelineViewportStateCreateInfo viewportInfo {
		.flags = {},
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr
	};

	// Primitives -> Fragments (pixels)
	vk::PipelineRasterizationStateCreateInfo rasterizerInfo {
		.flags = {},
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eBack,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = VK_FALSE,
		.depthBiasConstantFactor = 0.0f,
		.depthBiasClamp = 0.0f,
		.depthBiasSlopeFactor = 0.0f,
		.lineWidth = 1.0f
	};

	// MSAA
	vk::PipelineMultisampleStateCreateInfo multisamplingInfo {
		.flags = {},
		.rasterizationSamples = m_eUserMSAA,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	//vk::PipelineDepthStencilStateCreateInfo depthStencilInfo {};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment {
		.blendEnable = graphicsSettings.eColorBlending == ColorBlendingOptions::MIX ? VK_TRUE : VK_FALSE,
		.srcColorBlendFactor = vk::BlendFactor::eOne,
		.dstColorBlendFactor = vk::BlendFactor::eZero,
		.colorBlendOp = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	vk::PipelineColorBlendStateCreateInfo colorBlendInfo {
		.flags = {},
		.logicOpEnable = graphicsSettings.eColorBlending == ColorBlendingOptions::BITWISE ? VK_TRUE : VK_FALSE,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
		.blendConstants = std::array<float, 4> { 0.0f, 0.0f, 0.0f, 0.0f }
	};

	std::vector<vk::DynamicState> dynamicStates { 
		vk::DynamicState::eViewport, 
		vk::DynamicState::eScissor 
	};
	vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
		.flags = {},
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.flags = {},
		.setLayoutCount = 1,
		.pSetLayouts = &m_pDescriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};
	if (m_pLogicalDevice.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pPipelineLayout) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create pipeline layout!");

	vk::GraphicsPipelineCreateInfo pipelineInfo {
		.flags = {},
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssemblyInfo,
		.pTessellationState = nullptr,
		.pViewportState = &viewportInfo,
		.pRasterizationState = &rasterizerInfo,
		.pMultisampleState = &multisamplingInfo,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &colorBlendInfo,
		.pDynamicState = &dynamicStateInfo,
		.layout = m_pPipelineLayout,
		.renderPass = m_pRenderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};
	if (m_pLogicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pGraphicsPipeline) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create graphics pipeline!");

	// cleanup shader modules
	m_pLogicalDevice.destroyShaderModule(m_pFragmentShaderModule, nullptr);
	m_pLogicalDevice.destroyShaderModule(m_pVertexShaderModule, nullptr);

	LOG_SUCCESS("Graphics pipeline created successfully!");
}

void Graphics::CreateFrameBuffers()
{
	LOG_INFO("Creating frame buffers for swapchain image views...");

	m_vFrameBuffers = std::vector<vk::Framebuffer>(m_vSwapChainImageViews.size());

	for (auto i = 0; i < m_vSwapChainImageViews.size(); i++)
	{
		//std::vector<vk::ImageView> attachments { m_vSwapChainImageViews[i] };

		vk::FramebufferCreateInfo framebufferInfo {
			.flags = {},
			.renderPass = m_pRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_vSwapChainImageViews[i],
			.width = m_sSwapChainInfo.imageExtent.width,
			.height = m_sSwapChainInfo.imageExtent.height,
			.layers = 1
		};

		if (m_pLogicalDevice.createFramebuffer(&framebufferInfo, nullptr, &m_vFrameBuffers[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create framebuffer!");
	}

	LOG_SUCCESS("Frame buffers created successfully!");
}

void Graphics::CreateCommandPools()
{
	LOG_INFO("Creating command pools...");

	vk::CommandPoolCreateInfo drawingPoolInfo {
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = m_uiDeviceGraphicsIndex
	};
	
	if (m_pLogicalDevice.createCommandPool(&drawingPoolInfo, nullptr, &m_pDrawingCommandPool) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create drawing command pool!");

	vk::CommandPoolCreateInfo transferPoolInfo {
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = m_uiDeviceTransferIndex
	};

	if (m_pLogicalDevice.createCommandPool(&transferPoolInfo, nullptr, &m_pTransferCommandPool) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create transfer command pool!");

	LOG_SUCCESS("Successfully created command pools!");
}

void Graphics::CreateCommandBuffers()
{
	LOG_INFO("Creating command buffer...");

	m_vCommandBuffers = std::vector<vk::CommandBuffer>(m_ciMaxFramesInFlight);

	vk::CommandBufferAllocateInfo allocateInfo {
		.commandPool = m_pDrawingCommandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = static_cast<uint32_t>(m_vCommandBuffers.size())
	};

	if (m_pLogicalDevice.allocateCommandBuffers(&allocateInfo, m_vCommandBuffers.data()) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate command buffers!");

	LOG_SUCCESS("Created command buffer successfully!");
}

void Graphics::RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer)
{
	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = nullptr
	};

	if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		throw std::runtime_error("Failed begin on command buffer!");

	vk::ClearValue clearColor {
		.color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
	vk::Rect2D renderArea {
		.offset = {0, 0},
		.extent = m_sSwapChainInfo.imageExtent
	};
	vk::RenderPassBeginInfo renderPassInfo {
		.renderPass = m_pRenderPass,
		.framebuffer = frameBuffer,
		.renderArea = renderArea,
		.clearValueCount = 1,
		.pClearValues = &clearColor
	};
	commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pGraphicsPipeline);

	vk::Viewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_sSwapChainInfo.imageExtent.width),
		.height = static_cast<float>(m_sSwapChainInfo.imageExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	commandBuffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor {
		.offset = { 0, 0 },
		.extent = m_sSwapChainInfo.imageExtent
	};
	commandBuffer.setScissor(0, 1, &scissor);

	std::vector<vk::Buffer> vertexBuffers = { m_pVertexBuffer };
	std::vector<vk::DeviceSize> offsets = { 0 };
	commandBuffer.bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());

	commandBuffer.bindIndexBuffer(m_pIndexBuffer, 0, vk::IndexType::eUint16);

	UpdateUniformBuffer(m_uiCurrentFrame);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pPipelineLayout,
		0, 1, &m_vDescriptorSets[m_uiCurrentFrame], 0, nullptr);

	commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	commandBuffer.endRenderPass();

	commandBuffer.end();
}

void Graphics::DrawFrame()
{
	if (m_pLogicalDevice.waitForFences(1, &m_vFrameInFlightFences[m_uiCurrentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("waitForFences failed!");

	uint32_t imageIndex = 0;
	vk::Result result = m_pLogicalDevice.acquireNextImageKHR(m_pSwapChain, UINT64_MAX,
		m_vImageAvailableSemaphores[m_uiCurrentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
		return;
	}
	else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	if (m_pLogicalDevice.resetFences(1, &m_vFrameInFlightFences[m_uiCurrentFrame]) != vk::Result::eSuccess)
		throw std::runtime_error("resetFences failed!");

	m_vCommandBuffers[m_uiCurrentFrame].reset();
	RecordCommandBuffer(m_vCommandBuffers[m_uiCurrentFrame], m_vFrameBuffers[imageIndex]);

	std::vector<vk::Semaphore> waitSemaphores = { m_vImageAvailableSemaphores[m_uiCurrentFrame] };
	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput }; 
	std::vector<vk::Semaphore> signalSemaphores = { m_vRenderFinishedSemaphores[imageIndex] };
	
	vk::SubmitInfo submitInfo {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &m_vCommandBuffers[m_uiCurrentFrame],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores.data()
	};

	if (m_pGraphicsQueue.submit(1, &submitInfo, m_vFrameInFlightFences[m_uiCurrentFrame]) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit draw command buffer!");

	std::vector<vk::SwapchainKHR> swapChains = { m_pSwapChain };

	vk::PresentInfoKHR presentInfo {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = swapChains.data(),
		.pImageIndices = &imageIndex,
		.pResults = nullptr
	};
	
	result = m_pGraphicsQueue.presentKHR(&presentInfo);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_bFramebufferResized)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
		m_bFramebufferResized = false;
	}
	else if (result != vk::Result::eSuccess)
		throw std::runtime_error("failed to present swap chain image!");

	m_uiCurrentFrame = ++m_uiCurrentFrame % m_ciMaxFramesInFlight;
}

void Graphics::CreateSyncObjects()
{
	LOG_INFO("Creating synchronization objects...");

	vk::SemaphoreCreateInfo semaphoreInfo {
		.flags = {}
	};

	m_vImageAvailableSemaphores = std::vector<vk::Semaphore>(m_ciMaxFramesInFlight, VK_NULL_HANDLE);
	m_vFrameInFlightFences = std::vector<vk::Fence>(m_ciMaxFramesInFlight, VK_NULL_HANDLE);

	vk::FenceCreateInfo fenceInfo {
		.flags = vk::FenceCreateFlagBits::eSignaled
	};

	for (auto i = 0; i < m_ciMaxFramesInFlight; i++)
	{
		if (m_pLogicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_vImageAvailableSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create image available semaphore!");
		if (m_pLogicalDevice.createFence(&fenceInfo, nullptr, &m_vFrameInFlightFences[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create in flight fence!");
	}

	m_vRenderFinishedSemaphores = std::vector<vk::Semaphore>(m_vSwapChainImages.size(), VK_NULL_HANDLE);

	for (auto i = 0; i < m_vSwapChainImages.size(); i++)
		if (m_pLogicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_vRenderFinishedSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create render finished semaphore for swap chain image!");

	LOG_SUCCESS("Synchronization objects created successfully!");
}

void Graphics::RecreateSwapChain()
{
	LOG_INFO("Recreating swapchain...");

	int width = 0, height = 0;
	glfwGetFramebufferSize(m_pWindow, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_pWindow, &width, &height);
		glfwWaitEvents();
	}

	m_pLogicalDevice.waitIdle();

	CleanupSwapChain();
	CreateSwapChain();
	CreateImageViews();
	CreateFrameBuffers();

	LOG_SUCCESS("Swapchain recreated successfully!");
}

void Graphics::CleanupSwapChain()
{
	LOG_INFO("Cleaning up swapchain...");

	for (const auto& framebuffer : m_vFrameBuffers)
		m_pLogicalDevice.destroyFramebuffer(framebuffer, nullptr);

	for (const auto& imageView : m_vSwapChainImageViews)
		m_pLogicalDevice.destroyImageView(imageView, nullptr);

	m_pLogicalDevice.destroySwapchainKHR(m_pSwapChain, nullptr);

	LOG_SUCCESS("Swapchain cleaned up successfully!");
}

void Graphics::CreateVertexBuffer()
{
	LOG_INFO("Creating vertex buffer...");

	vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	vk::Buffer stagingBuffer = nullptr;
	vk::DeviceMemory stagingBufferMemory = nullptr;
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
	
	void* data;
	if (m_pLogicalDevice.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory for vertex buffer!");
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	m_pLogicalDevice.unmapMemory(stagingBufferMemory);

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_pVertexBuffer, m_pVertexBufferMemory);
	CopyBuffer(stagingBuffer, m_pVertexBuffer, bufferSize);
	m_pLogicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_pLogicalDevice.freeMemory(stagingBufferMemory, nullptr);

	LOG_SUCCESS("Vertex buffer created successfully!");
}

void Graphics::CreateIndexBuffer()
{
	LOG_INFO("Creating index buffer...");

	vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);
	
	void* data;
	if (m_pLogicalDevice.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory to index buffer!");
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	m_pLogicalDevice.unmapMemory(stagingBufferMemory);

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_pIndexBuffer, m_pIndexBufferMemory);
	CopyBuffer(stagingBuffer, m_pIndexBuffer, bufferSize);
	m_pLogicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_pLogicalDevice.freeMemory(stagingBufferMemory, nullptr);

	LOG_SUCCESS("Index buffer created successfully!");
}

uint32_t Graphics::FindMemoryTypeIndex(uint32_t memoryTypeBitMask, vk::MemoryPropertyFlags memoryProperties)
{
	LOG_INFO("Finding memory type...");

	for (uint32_t i = 0; i < m_sPhysicalDeviceMemoryProps.memoryTypeCount; i++)
	{
		if (((1 << i) & memoryTypeBitMask) && (m_sPhysicalDeviceMemoryProps.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
		{
			LOG_SUCCESS("Memory type found!");
			return i;
		}
	}

	LOG_FATAL("Failed to find suitable memory type!");
	return 0;
}

void Graphics::CreateBuffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags bufferUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, vk::Buffer& buffer, vk::DeviceMemory& deviceMemory)
{
	LOG_INFO("Creating buffer...");

	vk::BufferCreateInfo bufferInfo {
		.size = bufferSize,
		.usage = bufferUsageFlags,
		.sharingMode = vk::SharingMode::eExclusive
	};

	if (m_pLogicalDevice.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create vertex buffer!");

	vk::MemoryRequirements memRequirements;
	m_pLogicalDevice.getBufferMemoryRequirements(buffer, &memRequirements);

	vk::MemoryAllocateInfo allocInfo {
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(memRequirements.memoryTypeBits, memoryPropertyFlags)
	};

	if (m_pLogicalDevice.allocateMemory(&allocInfo, nullptr, &deviceMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate vertex buffer memory!");

	m_pLogicalDevice.bindBufferMemory(buffer, deviceMemory, 0);

	LOG_SUCCESS("Buffer created successfully!");
}

void Graphics::CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize)
{
	vk::CommandBuffer commandBuffer = BeginSingleTimeCommands();

	vk::BufferCopy copyRegion {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferSize
	};

	commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(commandBuffer);
}

void Graphics::CreateDescriptorSetLayout()
{
	LOG_INFO("Creating descriptor set layout...");

	vk::DescriptorSetLayoutBinding uboLayoutBinding {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.pImmutableSamplers = nullptr
	};

	vk::DescriptorSetLayoutBinding samplerLayoutBinding {
		.binding = 1,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eFragment,
		.pImmutableSamplers = nullptr
	};

	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings { uboLayoutBinding, samplerLayoutBinding };

	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
		.pBindings = layoutBindings.data()
	};

	if (m_pLogicalDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &m_pDescriptorSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create descriptor set layout!");

	LOG_SUCCESS("Successfully created descriptor set layout!");
}

void Graphics::CreateUniformBuffers()
{
	LOG_INFO("Creating uniform buffers...");

	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	m_vUniformBuffers = std::vector<vk::Buffer>(m_ciMaxFramesInFlight);
	m_vUniformBuffersMemory = std::vector<vk::DeviceMemory>(m_ciMaxFramesInFlight);
	m_vUniformBuffersMapped = std::vector<void*>(m_ciMaxFramesInFlight);

	for (size_t i = 0; i < m_ciMaxFramesInFlight; i++)
	{
		CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			m_vUniformBuffers[i], m_vUniformBuffersMemory[i]);

		if (m_pLogicalDevice.mapMemory(m_vUniformBuffersMemory[i], 0, bufferSize, {}, &m_vUniformBuffersMapped[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to map user memory to uniform buffer!");
	}

	LOG_SUCCESS("Successfully created uniform buffers!");
}

void Graphics::UpdateUniformBuffer(uint32_t imageIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo {};
	ubo.model = glm::rotate(glm::mat4(1.f), duration * glm::radians(-90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.proj = glm::perspective(glm::radians(45.f),
		static_cast<float>(m_sSwapChainCapabilities.currentExtent.width) / static_cast<float>(m_sSwapChainCapabilities.currentExtent.width),
		0.1f, 10.f);

	ubo.proj[1][1] *= -1;

	memcpy(m_vUniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
}

void Graphics::CreateDescriptorPool()
{
	vk::DescriptorPoolSize uboPoolSize {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = static_cast<uint32_t>(m_ciMaxFramesInFlight)
	};

	vk::DescriptorPoolSize samplerPoolSize{
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = static_cast<uint32_t>(m_ciMaxFramesInFlight)
	};

	std::vector<vk::DescriptorPoolSize> poolSizes { uboPoolSize, samplerPoolSize };
	
	vk::DescriptorPoolCreateInfo poolInfo {
		.flags = {},
		.maxSets = static_cast<uint32_t>(m_ciMaxFramesInFlight),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	if (m_pLogicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_pDescriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor pool!");
}

void Graphics::CreateDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(m_ciMaxFramesInFlight, m_pDescriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = m_pDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(m_ciMaxFramesInFlight),
		.pSetLayouts = layouts.data()
	};

	m_vDescriptorSets = std::vector<vk::DescriptorSet>(m_ciMaxFramesInFlight);

	if (m_pLogicalDevice.allocateDescriptorSets(&allocInfo, m_vDescriptorSets.data()) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	for (size_t i = 0; i < m_ciMaxFramesInFlight; i++)
	{
		vk::DescriptorBufferInfo uboInfo {
			.buffer = m_vUniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject)
		};

		vk::DescriptorImageInfo imageInfo {
			.sampler = m_pTextureSampler,
			.imageView = m_pTextureImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::WriteDescriptorSet uboDescWrite {
			.dstSet = m_vDescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pImageInfo = nullptr,
			.pBufferInfo = &uboInfo,
			.pTexelBufferView = nullptr
		};

		vk::WriteDescriptorSet imageDescWrite {
			.dstSet = m_vDescriptorSets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &imageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr
		};

		std::vector<vk::WriteDescriptorSet> descriptorWrites { uboDescWrite, imageDescWrite };
		
		m_pLogicalDevice.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void Graphics::CreateTextureImage()
{
	LOG_INFO("Creating textures from images...");

	int imageWidth, imageHeight, texChannels;
	stbi_uc* pixelData = stbi_load("Engine/Textures/popcat.jpg", &imageWidth, &imageHeight, &texChannels, STBI_rgb_alpha);
	if (!pixelData)
		throw std::runtime_error("failed to load texture image!");

	vk::DeviceSize imageDataSize = static_cast<vk::DeviceSize>(imageWidth * imageHeight * 4); // 4 color channels per pixel)
	vk::Buffer stagingBuffer; vk::DeviceMemory stagingBufferMemory;
	CreateBuffer(imageDataSize, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	void* data;
	if (m_pLogicalDevice.mapMemory(stagingBufferMemory, 0, imageDataSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory for vertex buffer!");
	memcpy(data, pixelData, static_cast<size_t>(imageDataSize));
	m_pLogicalDevice.unmapMemory(stagingBufferMemory);
	stbi_image_free(pixelData);
	CreateImage(imageWidth, imageHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_pTextureImage,
		m_pTextureImageMemory);

	TransitionImageLayout(m_pTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	CopyBufferToImage(stagingBuffer, m_pTextureImage, static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight));
	TransitionImageLayout(m_pTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	m_pLogicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_pLogicalDevice.freeMemory(stagingBufferMemory, nullptr);

	LOG_SUCCESS("Successfully created textures from images!");
}

void Graphics::CreateImage(uint32_t imageWidth, uint32_t imageHeight, vk::Format imageFormat, vk::ImageTiling imageTiling,
	vk::ImageUsageFlags imageUsage, vk::MemoryPropertyFlags imageProperties, vk::Image& image, vk::DeviceMemory& imageMemory)
{
	LOG_INFO("Creating image...");

	vk::ImageCreateInfo imageInfo {
		.flags = {},
		.imageType = vk::ImageType::e2D,
		.format = imageFormat,
		.extent = vk::Extent3D { static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight), 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = imageTiling,
		.usage = imageUsage,
		.sharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = vk::ImageLayout::eUndefined
	};

	if (m_pLogicalDevice.createImage(&imageInfo, nullptr, &image) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create image for texture!");

	vk::MemoryRequirements imageMemRequirements = {};
	m_pLogicalDevice.getImageMemoryRequirements(image, &imageMemRequirements);

	vk::MemoryAllocateInfo imageAllocInfo {
		.allocationSize = imageMemRequirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(imageMemRequirements.memoryTypeBits, imageProperties)
	};

	if (m_pLogicalDevice.allocateMemory(&imageAllocInfo, nullptr, &imageMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate image memory for texture!");

	m_pLogicalDevice.bindImageMemory(image, imageMemory, 0);

	LOG_SUCCESS("Successfully created image!");
}

vk::CommandBuffer Graphics::BeginSingleTimeCommands()
{
	LOG_INFO("Creating and starting single time commands buffer!");

	vk::CommandBufferAllocateInfo bufferAllocInfo{
		.commandPool = m_pDrawingCommandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	vk::CommandBuffer commandBuffer {};
	if (m_pLogicalDevice.allocateCommandBuffers(&bufferAllocInfo, &commandBuffer) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate command buffer for image!");

	vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};

	if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		throw std::runtime_error("failed to begin recording into command buffer!");

	LOG_SUCCESS("Successfully created and began recording command buffer!");
	return commandBuffer;
}

void Graphics::EndSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
	LOG_INFO("Freeing and ending single time command buffer...");

	commandBuffer.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer
	};

	if (m_pGraphicsQueue.submit(1, &submitInfo, nullptr) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit command buffer to graphics queue!");

	m_pGraphicsQueue.waitIdle();

	m_pLogicalDevice.freeCommandBuffers(m_pDrawingCommandPool, 1, &commandBuffer);

	LOG_SUCCESS("Successfully freed and ended single time command buffer!");
}

void Graphics::TransitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	vk::CommandBuffer commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier {
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eNone,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	};

	vk::PipelineStageFlags srcStage{};
	vk::PipelineStageFlags desStage{};

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		desStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		desStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
		throw std::runtime_error("Unsupported layout transition!");

	commandBuffer.pipelineBarrier(srcStage, desStage,
		{}, 0, nullptr, 0, nullptr, 1, &barrier);

	EndSingleTimeCommands(commandBuffer);
}

void Graphics::CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t imageWidth, uint32_t imageHeight)
{
	vk::CommandBuffer commandBuffer = BeginSingleTimeCommands();

	vk::BufferImageCopy region {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		.imageOffset = vk::Offset3D{0, 0, 0},
		.imageExtent = vk::Extent3D{imageWidth, imageHeight, 1}
	};

	commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

	EndSingleTimeCommands(commandBuffer);
}

void Graphics::CreateTextureImageView()
{
	m_pTextureImageView = CreateImageView(m_pTextureImage, vk::Format::eR8G8B8A8Srgb);
}

vk::ImageView Graphics::CreateImageView(vk::Image image, vk::Format format)
{
	vk::ImageViewCreateInfo imageViewInfo {
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	};

	vk::ImageView imageView = nullptr;
	if (m_pLogicalDevice.createImageView(&imageViewInfo, nullptr, &imageView) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create texture image view!");

	return imageView;
}

void Graphics::CreateTextureSampler()
{
	vk::SamplerCreateInfo samplerInfo {
		.flags = {},
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.f,
		.anisotropyEnable = m_sPhysicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = m_sPhysicalDeviceProps.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.f,
		.maxLod = 0.f,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	if (m_pLogicalDevice.createSampler(&samplerInfo, nullptr, &m_pTextureSampler) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create texture sampler!");
}

void Graphics::CreateWindowSurface()
{
	LOG_INFO("Creating window surface...");

	VkSurfaceKHR pSurface = nullptr;
	if (glfwCreateWindowSurface(m_pVulkanInstance, m_pWindow, nullptr, &pSurface) != VK_SUCCESS)
		throw std::runtime_error("failed to create window surface!");
	m_pSurface = pSurface;

	LOG_SUCCESS("Window surface created successfully!");
}

bool Graphics::IsDeviceSuitable(const vk::PhysicalDevice& device)
{
	LOG_INFO("Checking if device is suitable...");

	device.getProperties(&m_sPhysicalDeviceProps);
	if (m_sPhysicalDeviceProps.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
	{
		LOG_WARNING("Device is not a discrete GPU, skipping device...");
		return false;
	}

	// get max GPU samples for MSAA
	vk::SampleCountFlags sampleCountFlags = m_sPhysicalDeviceProps.limits.framebufferColorSampleCounts & m_sPhysicalDeviceProps.limits.framebufferDepthSampleCounts;
	if (sampleCountFlags & vk::SampleCountFlagBits::e64)
		m_eMaxMSAA = vk::SampleCountFlagBits::e64;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e32)
		m_eMaxMSAA = vk::SampleCountFlagBits::e32;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e16)
		m_eMaxMSAA = vk::SampleCountFlagBits::e16;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e8)
		m_eMaxMSAA = vk::SampleCountFlagBits::e8;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e4)
		m_eMaxMSAA = vk::SampleCountFlagBits::e4;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e2)
		m_eMaxMSAA = vk::SampleCountFlagBits::e2;

	device.getFeatures(&m_sPhysicalDeviceFeatures);

	if (!m_sPhysicalDeviceFeatures.geometryShader)
	{
		LOG_WARNING("Device does not support geometry shaders, skipping device...");
		return false;
	}

	// get device queue families & check if a queue family contain present and graphics support
	// find a queue family that supports both graphics and present operations for best performance
	// otherwise, we would need to create two separate queues, one for graphics and one for present
	uint32_t queueFamilyCount = 0;
	device.getQueueFamilyProperties(&queueFamilyCount, nullptr);
	if (!queueFamilyCount)
	{
		LOG_WARNING("No queue families found for the device, skipping device...");
		return false;
	}

	m_vPhysicalDeviceQueueFamiliesProps = std::vector<vk::QueueFamilyProperties>(queueFamilyCount);
	device.getQueueFamilyProperties(&queueFamilyCount, m_vPhysicalDeviceQueueFamiliesProps.data());
	auto queueFamilyProperties = std::vector<std::vector<bool>>(queueFamilyCount);

	int i = 0;
	bool presentQueueFound = false, graphicsQueueFound = false, transferQueueFound = false;
	for (const auto& queueFamily : m_vPhysicalDeviceQueueFamiliesProps)
	{
		std::vector<bool> queueFamiliesFound = { false, false, false }; // graphics, compute, present

		if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute)
		{
			queueFamiliesFound[0] = true; // has graphics support
			graphicsQueueFound = true;
			m_uiDeviceGraphicsIndex = i; 
		}

		if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer)
		{
			queueFamiliesFound[1] = true; // has graphics support
			transferQueueFound = true;
			m_uiDeviceTransferIndex = i;
		}

		vk::Bool32 windowPresentSupport = false;
		if (device.getSurfaceSupportKHR(i, m_pSurface, &windowPresentSupport) != vk::Result::eSuccess)
			throw std::runtime_error("getSurfaceSupportKHR failed!");

		if (windowPresentSupport)
		{
			queueFamiliesFound[2] = true; // has present support
			presentQueueFound = true;
			m_uiDevicePresentIndex = i;
		}

		//if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
		//	LOG_DEBUG("VK_QUEUE_COMPUTE_BIT found!");

		//if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
		//	LOG_DEBUG("VK_QUEUE_SPARSE_BINDING_BIT found!");

		//if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT)
		//	LOG_DEBUG("VK_QUEUE_PROTECTED_BIT found!");

		//if (queueFamily.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
		//	LOG_DEBUG("VK_QUEUE_VIDEO_DECODE_BIT_KHR found!");

		//if (queueFamily.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
		//	LOG_DEBUG("VK_QUEUE_VIDEO_ENCODE_BIT_KHR found!");

		//if (queueFamily.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
		//	LOG_DEBUG("VK_QUEUE_OPTICAL_FLOW_BIT_NV found!");

		//if (queueFamily.queueFlags & VK_QUEUE_FLAG_BITS_MAX_ENUM)
		//	LOG_DEBUG("VK_QUEUE_FLAG_BITS_MAX_ENUM found!");

		queueFamilyProperties[i++] = queueFamiliesFound;
	}

	if (!graphicsQueueFound)
	{
		LOG_WARNING("Graphics queue not found, skipping device...");
		return false;
	}
	if (!presentQueueFound)
	{
		LOG_WARNING("Present queue not found, skipping device...");
		return false;
	}
	if (!transferQueueFound)
	{
		LOG_WARNING("Transfer queue not found, skipping device...");
		return false;
	}

	// Check if there is a queue family that supports both graphics and present operations (best performance)
	i = 0;
	for (const auto& familySupport : queueFamilyProperties)
	{
		if (familySupport[0] && familySupport[1] && familySupport[2])
		{
			m_uiDeviceGraphicsIndex = i;
			m_uiDevicePresentIndex = i;
			m_uiDeviceTransferIndex = i;
			break;
		}
		i++;
	}

	// get device extensions & check if required extensions are supported
	uint32_t extensionCount = 0;
	
	if (device.enumerateDeviceExtensionProperties(nullptr, &extensionCount, nullptr) != vk::Result::eSuccess || !extensionCount)
	{
		LOG_WARNING("enumerateDeviceExtensionProperties failed!");
		return false;
	}
	m_vPhysicalDeviceExtensions = std::vector<vk::ExtensionProperties>(extensionCount);
	if (device.enumerateDeviceExtensionProperties(nullptr, &extensionCount, m_vPhysicalDeviceExtensions.data()) != vk::Result::eSuccess || !extensionCount)
	{
		LOG_WARNING("enumerateDeviceExtensionProperties failed!");
		return false;
	}

	std::set<std::string> requiredExtensions(m_vRequiredDeviceExtensionNames.begin(), m_vRequiredDeviceExtensionNames.end());
	for (const auto& extension : m_vPhysicalDeviceExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	if (!requiredExtensions.empty())
	{
		LOG_WARNING("Required device extensions not found:");
		LOG_WARNING(std::vector<std::string>(requiredExtensions.begin(), requiredExtensions.end()));
		LOG_WARNING("Skipping device...");
		return false;
	}

	// Get device swapchain color formats
	uint32_t formatCount;
	if (device.getSurfaceFormatsKHR(m_pSurface, &formatCount, nullptr) != vk::Result::eSuccess || !formatCount)
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_vSwapChainFormats = std::vector<vk::SurfaceFormatKHR>(formatCount);
	if (device.getSurfaceFormatsKHR(m_pSurface, &formatCount, m_vSwapChainFormats.data()) != vk::Result::eSuccess || !formatCount)
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}

	// Get device swapchain present modes
	uint32_t presentModeCount;
	if (device.getSurfacePresentModesKHR(m_pSurface, &presentModeCount, nullptr) != vk::Result::eSuccess || !presentModeCount)
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_vSwapChainPresentModes = std::vector<vk::PresentModeKHR>(presentModeCount);
	if (device.getSurfacePresentModesKHR(m_pSurface, &presentModeCount, m_vSwapChainPresentModes.data()) != vk::Result::eSuccess || !presentModeCount)
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}

	// Get device swapchain surface capabilities for swap extent (resolution) and image count
	if (device.getSurfaceCapabilitiesKHR(m_pSurface, &m_sSwapChainCapabilities) != vk::Result::eSuccess)
		throw std::runtime_error("getSurfaceCapabilitiesKHR failed!");

	// Get device's memory properties
	device.getMemoryProperties(&m_sPhysicalDeviceMemoryProps);

	LOG_SUCCESS("Device is suitable!");
	return true;
}

void Graphics::InitializeWindow()
{
	LOG_INFO("Initializing GLFW window...");

	LOG_INFO("Initializing GLFW...");
	if (!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");

	LOG_SUCCESS("GLFW initialized successfully");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	auto settings = Settings::Get()->GetGraphicsSettings();

	m_pWindow = glfwCreateWindow(settings.uiWindowWidth, settings.uiWindowHeight, settings.sWindowName.c_str(), nullptr, nullptr);
	if (m_pWindow == nullptr)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizeCallback);

	LOG_SUCCESS("GLFW window created successfully");
}

void Graphics::FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto graphics = static_cast<Graphics*>(glfwGetWindowUserPointer(window));
	if (!graphics)
	{
		LOG_FATAL("failed to get pointer to graphics object!");
		return;
	}

	graphics->m_bFramebufferResized = true;
}

void Graphics::CleanupVulkan()
{
	LOG_INFO("Cleaning up Vulkan...");

	CleanupSwapChain();

	m_pLogicalDevice.destroySampler(m_pTextureSampler, nullptr);
	m_pLogicalDevice.destroyImageView(m_pTextureImageView, nullptr);

	m_pLogicalDevice.destroyImage(m_pTextureImage, nullptr);
	m_pLogicalDevice.freeMemory(m_pTextureImageMemory, nullptr);

	for (size_t i = 0; i < m_ciMaxFramesInFlight; i++)
	{
		m_pLogicalDevice.destroyBuffer(m_vUniformBuffers[i], nullptr);
		m_pLogicalDevice.freeMemory(m_vUniformBuffersMemory[i], nullptr);
	}

	m_pLogicalDevice.destroyDescriptorPool(m_pDescriptorPool, nullptr);

	m_pLogicalDevice.destroyDescriptorSetLayout(m_pDescriptorSetLayout, nullptr);

	m_pLogicalDevice.destroyBuffer(m_pIndexBuffer, nullptr);
	m_pLogicalDevice.freeMemory(m_pIndexBufferMemory, nullptr);

	m_pLogicalDevice.destroyBuffer(m_pVertexBuffer, nullptr);
	m_pLogicalDevice.freeMemory(m_pVertexBufferMemory, nullptr);

	m_pLogicalDevice.destroyPipeline(m_pGraphicsPipeline, nullptr);
	m_pLogicalDevice.destroyPipelineLayout(m_pPipelineLayout, nullptr);
	
	m_pLogicalDevice.destroyRenderPass(m_pRenderPass, nullptr);
	
	for (auto i = 0; i < m_vRenderFinishedSemaphores.size(); i++)
		m_pLogicalDevice.destroySemaphore(m_vRenderFinishedSemaphores[i], nullptr);

	for (auto i = 0; i < m_ciMaxFramesInFlight; i++)
	{
		m_pLogicalDevice.destroyFence(m_vFrameInFlightFences[i], nullptr);
		m_pLogicalDevice.destroySemaphore(m_vImageAvailableSemaphores[i], nullptr);
	}

	m_pLogicalDevice.destroyCommandPool(m_pDrawingCommandPool, nullptr);
	m_pLogicalDevice.destroyCommandPool(m_pTransferCommandPool, nullptr);

	m_pLogicalDevice.destroy(nullptr);

#ifdef _DEBUG
	CleanupDebugger();
#endif // _DEBUG

	m_pVulkanInstance.destroySurfaceKHR(m_pSurface, nullptr);
	m_pVulkanInstance.destroy(nullptr);

	glfwDestroyWindow(m_pWindow);
	glfwTerminate();

	LOG_SUCCESS("Vulkan cleaned up successfully!");
}

void Graphics::InitializeDebugger()
{
	LOG_INFO("Initializing Vulkan debugger...");

	m_pDebugMessenger = m_pVulkanInstance.createDebugUtilsMessengerEXT(m_sDebugCreateInfo);

	//if (m_pVulkanInstance.createDebugUtilsMessengerEXT(&m_sDebugCreateInfo, nullptr, &m_pDebugMessenger) != vk::Result::eSuccess)
	//	throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed!");

	LOG_SUCCESS("Vulkan debugger initialized successfully!");
}

void Graphics::CleanupDebugger()
{
	LOG_INFO("Cleaning up Vulkan debugger...");

	m_pVulkanInstance.destroyDebugUtilsMessengerEXT(m_pDebugMessenger, nullptr);

	LOG_SUCCESS("Vulkan debugger cleaned up successfully!");
}

bool Graphics::IsDebugSupported()
{
	LOG_INFO("Checking if Vulkan debugging is supported...");

	for (const auto& layerName : m_vRequiredValidationLayerNames)
		for (const auto& layer : m_vVulkanLayerProps)
		{
			if (!strcmp(layerName, layer.layerName))
			{
				LOG_SUCCESS("Vulkan debugging is supported!");
				return true;
			}
		}

	LOG_WARNING("Vulkan debugging is not supported!");
	return false;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Graphics::DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	switch (messageSeverity)
	{
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
			LOG_DEBUG(std::string(pCallbackData->pMessage));
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
			LOG_INFO(std::string(pCallbackData->pMessage));
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
			LOG_WARNING(std::string(pCallbackData->pMessage));
			break;
		case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
			LOG_FATAL(std::string(pCallbackData->pMessage));
			break;
		default:
			LOG_DEBUG(std::string(pCallbackData->pMessage));
			break;
	}
	
	return VK_FALSE;
}