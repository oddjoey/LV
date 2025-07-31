#include "Graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

std::shared_ptr<Graphics> Graphics::s_instance = nullptr;

std::shared_ptr<Graphics> Graphics::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<Graphics>(new Graphics());

	return s_instance;
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

	CreateCommandPools();

	CreateDepthResources();

	CreateFrameBuffers();

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

	createInfo.enabledLayerCount = static_cast<uint32_t>(m_requiredValidationLayerNames.size());
	createInfo.ppEnabledLayerNames = m_requiredValidationLayerNames.data();

	m_debugCreateInfo.sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT;
	m_debugCreateInfo.messageSeverity = 
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	m_debugCreateInfo.messageType = 
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	m_debugCreateInfo.pfnUserCallback = DebugCallback;

	createInfo.pNext = &m_debugCreateInfo;
#endif // _DEBUG

	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size());
	createInfo.ppEnabledExtensionNames = requiredExtensionNames.data();

	if (vk::createInstance(&createInfo, nullptr, &m_vulkanInstance) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create vulkan instance");

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_vulkanInstance);

	LOG_SUCCESS("Vulkan instance created successfully!");
}

void Graphics::GetVulkanData()
{
	LOG_INFO("Getting Vulkan data...");
	
	uint32_t instanceExtensionCount = 0;
	
	if (vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr) != vk::Result::eSuccess || !instanceExtensionCount)
		throw std::runtime_error("enumerateInstanceExtensionProperties failed!");

	m_vulkanExtensionProps = std::vector<vk::ExtensionProperties>(instanceExtensionCount);
	if (vk::enumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, m_vulkanExtensionProps.data()) != vk::Result::eSuccess || !instanceExtensionCount)
		throw std::runtime_error("enumerateInstanceExtensionProperties failed!");

	uint32_t instanceLayerCount = 0;
	if (vk::enumerateInstanceLayerProperties(&instanceLayerCount, nullptr) != vk::Result::eSuccess || !instanceLayerCount)
		throw std::runtime_error("enumerateInstanceLayerProperties failed!");

	m_vulkanLayerProps = std::vector<vk::LayerProperties>(instanceLayerCount);
	if (vk::enumerateInstanceLayerProperties(&instanceLayerCount, m_vulkanLayerProps.data()) != vk::Result::eSuccess || !instanceLayerCount)
		throw std::runtime_error("enumerateInstanceLayerProperties failed!");

	LOG_SUCCESS("Vulkan data retrieved successfully!");
}

void Graphics::PickPhysicalDevice()
{
	LOG_INFO("Picking physical device (GPU)...");

	unsigned int deviceCount = 0;
	;
	if (m_vulkanInstance.enumeratePhysicalDevices(&deviceCount, nullptr) != vk::Result::eSuccess || deviceCount == 0)
		throw std::runtime_error("No device found with Vulkan support");

	auto devices = std::vector<vk::PhysicalDevice>(deviceCount);
	if (m_vulkanInstance.enumeratePhysicalDevices(&deviceCount, devices.data()) != vk::Result::eSuccess || deviceCount == 0)
		throw std::runtime_error("No device found with Vulkan support");

	for (const auto& device : devices)
		if (IsDeviceSuitable(device))
		{
			m_physicalDevice = device;
			break;
		}

	if (m_physicalDevice == nullptr)
		throw std::runtime_error("Failed to find a suitable device");

	LOG_SUCCESS("Physical device (GPU) picked successfully!");
}

void Graphics::CreateLogicalDevice()
{
	LOG_INFO("Creating logical device...");
	std::vector<vk::DeviceQueueCreateInfo> deviceQueuesInfo;
	std::set<uint32_t> uniqueQueueFamilies { m_physicalDeviceGraphicsIndex, m_physicalDevicePresentIndex, m_physicalDeviceTransferIndex };

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
	deviceFeatures.depthBounds = vk::True;

	vk::DeviceCreateInfo deviceCreateInfo {
		.flags = {},
		.queueCreateInfoCount = static_cast<uint32_t>(deviceQueuesInfo.size()),
		.pQueueCreateInfos = deviceQueuesInfo.data(),
		.enabledExtensionCount = static_cast<uint32_t>(m_requiredDeviceExtensionNames.size()),
		.ppEnabledExtensionNames = m_requiredDeviceExtensionNames.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

//#ifdef _DEBUG
//	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(m_cvRequiredValidationLayerNames.size());
//	deviceCreateInfo.ppEnabledLayerNames = m_cvRequiredValidationLayerNames.data();
//#endif // _DEBUG

	if (m_physicalDevice.createDevice(&deviceCreateInfo, nullptr, &m_logicalDevice) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create logical device!");
	
	m_logicalDevice.getQueue(m_physicalDeviceGraphicsIndex, 0, &m_queueGraphics);
	m_logicalDevice.getQueue(m_physicalDevicePresentIndex, 0, &m_queuePresent);
	m_logicalDevice.getQueue(m_physicalDeviceTransferIndex, 0, &m_queueTransfer);

	LOG_SUCCESS("Logical device created successfully!");
}

void Graphics::CreateSwapChain()
{
	LOG_INFO("Creating swap chain...");

	m_swapChainFormat = m_swapChainFormats[0];
	bool formatFound = false;
	for (auto& format : m_swapChainFormats)
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			formatFound = true;
			m_swapChainFormat = format;
			break;
		}
	if (!formatFound)
		LOG_WARNING("Requested swapchain format/color space not found on device, using device's first format/color format...");

	auto graphicsSettings = Settings::Get()->GetGraphicsSettings();
	switch (graphicsSettings.eVSync)
	{
		case VSyncOptions::NONE:
			m_userPresentMode = vk::PresentModeKHR::eImmediate; // No VSync
			LOG_DEBUG("VSyncOptions::NONE / VK_PRESENT_MODE_IMMEDIATE_KHR");
			break;
		case VSyncOptions::ENABLED:
			m_userPresentMode = vk::PresentModeKHR::eFifo; // VSync
			LOG_DEBUG("VSyncOptions::ENABLED / VK_PRESENT_MODE_FIFO_KHR");
			break;
		case VSyncOptions::RELAXED:
			m_userPresentMode = vk::PresentModeKHR::eFifoRelaxed; // Relaxed VSync
			LOG_DEBUG("VSyncOptions::RELAXED / VK_PRESENT_MODE_FIFO_RELAXED_KHR");
			break;
		case VSyncOptions::MAILBOX:
			m_userPresentMode = vk::PresentModeKHR::eMailbox; // Mailbox VSync
			LOG_DEBUG("VSyncOptions::MAILBOX / VK_PRESENT_MODE_MAILBOX_KHR");
			break;
	}

	vk::PresentModeKHR swapChainPresentMode = vk::PresentModeKHR::eFifo;
	bool presentFound = false;
	for (const auto& presentMode : m_swapChainPresentModes)
		if (presentMode == m_userPresentMode)
		{
			presentFound = true;
			swapChainPresentMode = presentMode;
			break;
		}
	if (!presentFound)
		LOG_WARNING("Requested present mode not found on device, using defaults (FIFO / VYSNC) as its guaranteed...");

	vk::Extent2D swapChainExtent = {};
	if (m_swapChainCapabilities.currentExtent.width == UINT32_MAX)
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), m_swapChainCapabilities.minImageExtent.width, m_swapChainCapabilities.maxImageExtent.width);
		swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), m_swapChainCapabilities.minImageExtent.height, m_swapChainCapabilities.maxImageExtent.height);
	}
	else
		swapChainExtent = m_swapChainCapabilities.currentExtent;

	uint32_t swapChainImageCount = m_swapChainCapabilities.minImageCount + 1;
	if (m_swapChainCapabilities.maxImageCount > 0 && swapChainImageCount > m_swapChainCapabilities.maxImageCount)
	{
		LOG_WARNING("Swap chain image count exceeds maximum, clamping to maximum...");
		swapChainImageCount = m_swapChainCapabilities.maxImageCount;
	}

	m_swapChainInfo = {
		.flags = {},
		.surface = m_windowSurface,
		.minImageCount = swapChainImageCount,
		.imageFormat = m_swapChainFormat.format,
		.imageColorSpace = m_swapChainFormat.colorSpace,
		.imageExtent = swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment
	};

	if (m_physicalDeviceGraphicsIndex != m_physicalDevicePresentIndex || m_physicalDeviceGraphicsIndex != m_physicalDeviceTransferIndex)
	{
		LOG_WARNING("Needed features use separate queue families, using concurrent mode for swap chain images.");
		// If the graphics queue family is different from the present queue family, we need to specify the queue families
		std::set<uint32_t> uniqueQueueFamilyIndices = { m_physicalDeviceGraphicsIndex, m_physicalDevicePresentIndex, m_physicalDeviceTransferIndex };
		std::vector<uint32_t> queueFamilyIndices(uniqueQueueFamilyIndices.size());

		for (auto& index : uniqueQueueFamilyIndices)
			queueFamilyIndices.push_back(index);

		m_swapChainInfo.imageSharingMode = vk::SharingMode::eConcurrent; // images can be shared between multiple queues
		m_swapChainInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
		m_swapChainInfo.pQueueFamilyIndices = queueFamilyIndices.data();
	}
	else
	{
		LOG_INFO("Required features use the same queue family, using exclusive mode for swap chain images.");
		// If the graphics queue family is the same as the present queue family, we can use exclusive mode
		m_swapChainInfo.imageSharingMode = vk::SharingMode::eExclusive;
		m_swapChainInfo.queueFamilyIndexCount = 0;
		m_swapChainInfo.pQueueFamilyIndices = nullptr;
	}

	m_swapChainInfo.preTransform = m_swapChainCapabilities.currentTransform;
	m_swapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	// presentation mode (VSync)
	m_swapChainInfo.presentMode = swapChainPresentMode;
	// clipped swap chain images provide the best performance compared to non-clipped images
	// clipped images are not presented if they are obscured by other windows
	m_swapChainInfo.clipped = VK_TRUE;
	// if recreating swapchain, provide previous one here
	// TO-DO: Implement swap chain recreation logic
	m_swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (m_logicalDevice.createSwapchainKHR(&m_swapChainInfo, nullptr, &m_swapChain) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create swapchain!");

	// Get swap chain images
	if (m_logicalDevice.getSwapchainImagesKHR(m_swapChain, &swapChainImageCount, nullptr) != vk::Result::eSuccess || !swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	m_swapChainImages = std::vector<vk::Image>(swapChainImageCount);
	if (m_logicalDevice.getSwapchainImagesKHR(m_swapChain, &swapChainImageCount, m_swapChainImages.data()) != vk::Result::eSuccess || !swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	LOG_SUCCESS("Swapchain created successfully!");
}

void Graphics::CreateImageViews()
{
	LOG_INFO("Creating image views for swapchain images...");

	m_swapChainImageViews = std::vector<vk::ImageView>(m_swapChainImages.size());
	for (auto i = 0; i < m_swapChainImages.size(); i++)
		m_swapChainImageViews[i] = CreateImageView(m_swapChainImages[i], m_swapChainFormat.format, vk::ImageAspectFlagBits::eColor);

	LOG_SUCCESS("Image views for swapchain images created successfully!");
}

void Graphics::CreateRenderPass()
{
	LOG_INFO("Creating render pass...");

	auto settings = Settings::Get()->GetGraphicsSettings();
	switch (settings.eMSAA)
	{
	case MSAAOptions::X1:
		m_userMSAA = vk::SampleCountFlagBits::e1; // no multisampling
		LOG_DEBUG("MSAAOptions::X1 / VK_SAMPLE_COUNT_1_BIT");
		break;
	case MSAAOptions::X2:
		m_userMSAA = vk::SampleCountFlagBits::e2; // 2x multisampling
		LOG_DEBUG("MSAAOptions::X2 / VK_SAMPLE_COUNT_2_BIT");
		break;
	case MSAAOptions::X4:
		m_userMSAA = vk::SampleCountFlagBits::e4; // 4x multisampling
		LOG_DEBUG("MSAAOptions::X4 / VK_SAMPLE_COUNT_4_BIT");
		break;
	case MSAAOptions::X8:
		m_userMSAA = vk::SampleCountFlagBits::e8; // 8x multisampling
		LOG_DEBUG("MSAAOptions::X8 / VK_SAMPLE_COUNT_8_BIT");
		break;
	case MSAAOptions::X16:
		m_userMSAA = vk::SampleCountFlagBits::e16; // 16x multisampling
		LOG_DEBUG("MSAAOptions::X16 / VK_SAMPLE_COUNT_16_BIT");
		break;
	case MSAAOptions::X32:
		m_userMSAA = vk::SampleCountFlagBits::e32; // 32x multisampling
		LOG_DEBUG("MSAAOptions::X32 / VK_SAMPLE_COUNT_32_BIT");
		break;
	case MSAAOptions::X64:
		m_userMSAA = vk::SampleCountFlagBits::e64; // 64x multisampling
		LOG_DEBUG("MSAAOptions::X64 / VK_SAMPLE_COUNT_64_BIT");
		break;
	}

	vk::AttachmentReference colorAttachmentRef {
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};
	vk::AttachmentDescription colorAttachment {
		.flags = {},
		.format = m_swapChainInfo.imageFormat,
		.samples = m_userMSAA,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR
	};

	vk::AttachmentReference depthAttachmentRef {
		.attachment = 1,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::AttachmentDescription depthAttachment {
		.flags = {},
		.format = m_depthImageFormat,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};

	std::vector<vk::AttachmentDescription> attachments = { colorAttachment, depthAttachment };

	vk::SubpassDescription subpass { 
		.flags = {},
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = &depthAttachmentRef,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	vk::SubpassDependency subpassDependency {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
	};

	vk::RenderPassCreateInfo renderPassInfo {
		.flags = {},
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = 1,
		.pDependencies = &subpassDependency
	};

	if (m_logicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_renderPass) != vk::Result::eSuccess)
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
	if (m_logicalDevice.createShaderModule(&vertShaderInfo, nullptr, &m_shaderModuleVertex) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create vertex shader module!");
	auto fragShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/frag.spv");
	vk::ShaderModuleCreateInfo fragShaderInfo {
		.flags = {},
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data())
	};
	if (m_logicalDevice.createShaderModule(&fragShaderInfo, nullptr, &m_shaderModuleFragment) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create fragment shader module!");

	// Shader code
	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		.flags = {},
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = m_shaderModuleVertex,
		.pName = "main",
		.pSpecializationInfo = nullptr
	};
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
		.flags = {},
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = m_shaderModuleFragment,
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
		.rasterizationSamples = m_userMSAA,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 1.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	vk::PipelineDepthStencilStateCreateInfo depthStencilInfo {
		.flags = {},
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::True,
		.stencilTestEnable = vk::True,
		.front = {},
		.back = {},
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f,
	};

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
		.pSetLayouts = &m_descriptorSetLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};
	if (m_logicalDevice.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
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
		.pDepthStencilState = &depthStencilInfo,
		.pColorBlendState = &colorBlendInfo,
		.pDynamicState = &dynamicStateInfo,
		.layout = m_pipelineLayout,
		.renderPass = m_renderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};
	if (m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipelineGraphics) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create graphics pipeline!");

	// cleanup shader modules
	m_logicalDevice.destroyShaderModule(m_shaderModuleFragment, nullptr);
	m_logicalDevice.destroyShaderModule(m_shaderModuleVertex, nullptr);

	LOG_SUCCESS("Graphics pipeline created successfully!");
}

void Graphics::CreateFrameBuffers()
{
	LOG_INFO("Creating frame buffers for swapchain image views...");

	m_frameBuffers = std::vector<vk::Framebuffer>(m_swapChainImageViews.size());

	for (auto i = 0; i < m_swapChainImageViews.size(); i++)
	{
		std::vector<vk::ImageView> attachments { m_swapChainImageViews[i], m_depthImageView };

		vk::FramebufferCreateInfo framebufferInfo {
			.flags = {},
			.renderPass = m_renderPass,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.width = m_swapChainInfo.imageExtent.width,
			.height = m_swapChainInfo.imageExtent.height,
			.layers = 1
		};

		if (m_logicalDevice.createFramebuffer(&framebufferInfo, nullptr, &m_frameBuffers[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create framebuffer!");
	}

	LOG_SUCCESS("Frame buffers created successfully!");
}

void Graphics::CreateCommandPools()
{
	LOG_INFO("Creating command pools...");

	vk::CommandPoolCreateInfo drawingPoolInfo {
		.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = m_physicalDeviceGraphicsIndex
	};
	
	if (m_logicalDevice.createCommandPool(&drawingPoolInfo, nullptr, &m_commandPoolDraw) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create drawing command pool!");

	vk::CommandPoolCreateInfo transferPoolInfo {
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = m_physicalDeviceTransferIndex
	};

	if (m_logicalDevice.createCommandPool(&transferPoolInfo, nullptr, &m_commandPoolTransfer) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create transfer command pool!");

	LOG_SUCCESS("Successfully created command pools!");
}

void Graphics::CreateCommandBuffers()
{
	LOG_INFO("Creating command buffer...");

	m_commandBuffers = std::vector<vk::CommandBuffer>(m_maxFramesInFlight);

	vk::CommandBufferAllocateInfo allocateInfo {
		.commandPool = m_commandPoolDraw,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size())
	};

	if (m_logicalDevice.allocateCommandBuffers(&allocateInfo, m_commandBuffers.data()) != vk::Result::eSuccess)
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
	vk::ClearValue clearDepth {
		.depthStencil = { 1.f, 0 }
	};
	std::vector<vk::ClearValue> clearValues = { clearColor, clearDepth };
	vk::Rect2D renderArea {
		.offset = {0, 0},
		.extent = m_swapChainInfo.imageExtent
	};
	vk::RenderPassBeginInfo renderPassInfo {
		.renderPass = m_renderPass,
		.framebuffer = frameBuffer,
		.renderArea = renderArea,
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};
	commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelineGraphics);

	vk::Viewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_swapChainInfo.imageExtent.width),
		.height = static_cast<float>(m_swapChainInfo.imageExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	commandBuffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor {
		.offset = { 0, 0 },
		.extent = m_swapChainInfo.imageExtent
	};
	commandBuffer.setScissor(0, 1, &scissor);

	std::vector<vk::Buffer> vertexBuffers = { m_vertexBuffer };
	std::vector<vk::DeviceSize> offsets = { 0 };
	commandBuffer.bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());

	commandBuffer.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint16);

	UpdateUniformBuffer(m_frameInFlightIndex);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout,
		0, 1, &m_descriptorSets[m_frameInFlightIndex], 0, nullptr);

	commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	commandBuffer.endRenderPass();

	commandBuffer.end();
}

void Graphics::DrawFrame()
{
	if (m_logicalDevice.waitForFences(1, &m_frameInFlightFences[m_frameInFlightIndex], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("waitForFences failed!");

	uint32_t imageIndex = 0;
	vk::Result result = m_logicalDevice.acquireNextImageKHR(m_swapChain, UINT64_MAX,
		m_imageAvailableSemaphores[m_frameInFlightIndex], VK_NULL_HANDLE, &imageIndex);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
		return;
	}
	else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	if (m_logicalDevice.resetFences(1, &m_frameInFlightFences[m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("resetFences failed!");

	m_commandBuffers[m_frameInFlightIndex].reset();
	RecordCommandBuffer(m_commandBuffers[m_frameInFlightIndex], m_frameBuffers[imageIndex]);

	std::vector<vk::Semaphore> waitSemaphores = { m_imageAvailableSemaphores[m_frameInFlightIndex] };
	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput }; 
	std::vector<vk::Semaphore> signalSemaphores = { m_renderFinishedSemaphores[imageIndex] };
	
	vk::SubmitInfo submitInfo {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &m_commandBuffers[m_frameInFlightIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores.data()
	};

	if (m_queueGraphics.submit(1, &submitInfo, m_frameInFlightFences[m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit draw command buffer!");

	std::vector<vk::SwapchainKHR> swapChains = { m_swapChain };

	vk::PresentInfoKHR presentInfo {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = swapChains.data(),
		.pImageIndices = &imageIndex,
		.pResults = nullptr
	};
	
	result = m_queueGraphics.presentKHR(&presentInfo);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_framebufferResized)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
		m_framebufferResized = false;
	}
	else if (result != vk::Result::eSuccess)
		throw std::runtime_error("failed to present swap chain image!");

	m_frameInFlightIndex = ++m_frameInFlightIndex % m_maxFramesInFlight;
}

void Graphics::CreateSyncObjects()
{
	LOG_INFO("Creating synchronization objects...");

	vk::SemaphoreCreateInfo semaphoreInfo {
		.flags = {}
	};

	m_imageAvailableSemaphores = std::vector<vk::Semaphore>(m_maxFramesInFlight, VK_NULL_HANDLE);
	m_frameInFlightFences = std::vector<vk::Fence>(m_maxFramesInFlight, VK_NULL_HANDLE);

	vk::FenceCreateInfo fenceInfo {
		.flags = vk::FenceCreateFlagBits::eSignaled
	};

	for (auto i = 0; i < m_maxFramesInFlight; i++)
	{
		if (m_logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create image available semaphore!");
		if (m_logicalDevice.createFence(&fenceInfo, nullptr, &m_frameInFlightFences[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create in flight fence!");
	}

	m_renderFinishedSemaphores = std::vector<vk::Semaphore>(m_swapChainImages.size(), VK_NULL_HANDLE);

	for (auto i = 0; i < m_swapChainImages.size(); i++)
		if (m_logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create render finished semaphore for swap chain image!");

	LOG_SUCCESS("Synchronization objects created successfully!");
}

void Graphics::RecreateSwapChain()
{
	LOG_INFO("Recreating swapchain...");

	int width = 0, height = 0;
	glfwGetFramebufferSize(m_window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_window, &width, &height);
		glfwWaitEvents();
	}

	m_logicalDevice.waitIdle();

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateDepthResources();
	CreateFrameBuffers();

	LOG_SUCCESS("Swapchain recreated successfully!");
}

void Graphics::CleanupSwapChain()
{
	LOG_INFO("Cleaning up swapchain...");

	m_logicalDevice.destroyImageView(m_depthImageView, nullptr);
	m_logicalDevice.destroyImage(m_depthImage, nullptr);
	m_logicalDevice.freeMemory(m_depthImageMemory, nullptr);

	for (const auto& framebuffer : m_frameBuffers)
		m_logicalDevice.destroyFramebuffer(framebuffer, nullptr);

	for (const auto& imageView : m_swapChainImageViews)
		m_logicalDevice.destroyImageView(imageView, nullptr);

	m_logicalDevice.destroySwapchainKHR(m_swapChain, nullptr);

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
	if (m_logicalDevice.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory for vertex buffer!");
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	m_logicalDevice.unmapMemory(stagingBufferMemory);

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_vertexBuffer, m_vertexBufferMemory);
	CopyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
	m_logicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_logicalDevice.freeMemory(stagingBufferMemory, nullptr);

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
	if (m_logicalDevice.mapMemory(stagingBufferMemory, 0, bufferSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory to index buffer!");
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	m_logicalDevice.unmapMemory(stagingBufferMemory);

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_indexBuffer, m_indexBufferMemory);
	CopyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
	m_logicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_logicalDevice.freeMemory(stagingBufferMemory, nullptr);

	LOG_SUCCESS("Index buffer created successfully!");
}

uint32_t Graphics::FindMemoryTypeIndex(uint32_t memoryTypeBitMask, vk::MemoryPropertyFlags memoryProperties)
{
	LOG_INFO("Finding memory type...");

	for (uint32_t i = 0; i < m_physicalDeviceMemoryProps.memoryTypeCount; i++)
	{
		if (((1 << i) & memoryTypeBitMask) && (m_physicalDeviceMemoryProps.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
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

	if (m_logicalDevice.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create vertex buffer!");

	vk::MemoryRequirements memRequirements;
	m_logicalDevice.getBufferMemoryRequirements(buffer, &memRequirements);

	vk::MemoryAllocateInfo allocInfo {
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(memRequirements.memoryTypeBits, memoryPropertyFlags)
	};

	if (m_logicalDevice.allocateMemory(&allocInfo, nullptr, &deviceMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate vertex buffer memory!");

	m_logicalDevice.bindBufferMemory(buffer, deviceMemory, 0);

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

	if (m_logicalDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &m_descriptorSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create descriptor set layout!");

	LOG_SUCCESS("Successfully created descriptor set layout!");
}

void Graphics::CreateUniformBuffers()
{
	LOG_INFO("Creating uniform buffers...");

	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	m_uniformBuffers = std::vector<vk::Buffer>(m_maxFramesInFlight);
	m_uniformBuffersMemory = std::vector<vk::DeviceMemory>(m_maxFramesInFlight);
	m_uniformBuffersMapped = std::vector<void*>(m_maxFramesInFlight);

	for (size_t i = 0; i < m_maxFramesInFlight; i++)
	{
		CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			m_uniformBuffers[i], m_uniformBuffersMemory[i]);

		if (m_logicalDevice.mapMemory(m_uniformBuffersMemory[i], 0, bufferSize, {}, &m_uniformBuffersMapped[i]) != vk::Result::eSuccess)
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
		static_cast<float>(m_swapChainCapabilities.currentExtent.width) / static_cast<float>(m_swapChainCapabilities.currentExtent.width),
		0.1f, 10.f);

	ubo.proj[1][1] *= -1;

	memcpy(m_uniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
}

void Graphics::CreateDescriptorPool()
{
	vk::DescriptorPoolSize uboPoolSize {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = static_cast<uint32_t>(m_maxFramesInFlight)
	};

	vk::DescriptorPoolSize samplerPoolSize{
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = static_cast<uint32_t>(m_maxFramesInFlight)
	};

	std::vector<vk::DescriptorPoolSize> poolSizes { uboPoolSize, samplerPoolSize };
	
	vk::DescriptorPoolCreateInfo poolInfo {
		.flags = {},
		.maxSets = static_cast<uint32_t>(m_maxFramesInFlight),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	if (m_logicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor pool!");
}

void Graphics::CreateDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(m_maxFramesInFlight, m_descriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = m_descriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(m_maxFramesInFlight),
		.pSetLayouts = layouts.data()
	};

	m_descriptorSets = std::vector<vk::DescriptorSet>(m_maxFramesInFlight);

	if (m_logicalDevice.allocateDescriptorSets(&allocInfo, m_descriptorSets.data()) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	for (size_t i = 0; i < m_maxFramesInFlight; i++)
	{
		vk::DescriptorBufferInfo uboInfo {
			.buffer = m_uniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject)
		};

		vk::DescriptorImageInfo imageInfo {
			.sampler = m_textureSampler,
			.imageView = m_textureImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::WriteDescriptorSet uboDescWrite {
			.dstSet = m_descriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pImageInfo = nullptr,
			.pBufferInfo = &uboInfo,
			.pTexelBufferView = nullptr
		};

		vk::WriteDescriptorSet imageDescWrite {
			.dstSet = m_descriptorSets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &imageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr
		};

		std::vector<vk::WriteDescriptorSet> descriptorWrites { uboDescWrite, imageDescWrite };
		
		m_logicalDevice.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
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
	if (m_logicalDevice.mapMemory(stagingBufferMemory, 0, imageDataSize, {}, &data) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory for vertex buffer!");
	memcpy(data, pixelData, static_cast<size_t>(imageDataSize));
	m_logicalDevice.unmapMemory(stagingBufferMemory);
	stbi_image_free(pixelData);
	m_textureImage = CreateImage(imageWidth, imageHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal,
		m_textureImageMemory);

	TransitionImageLayout(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	CopyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight));
	TransitionImageLayout(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	m_logicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_logicalDevice.freeMemory(stagingBufferMemory, nullptr);

	LOG_SUCCESS("Successfully created textures from images!");
}

vk::Image Graphics::CreateImage(uint32_t imageWidth, uint32_t imageHeight, vk::Format imageFormat, vk::ImageTiling imageTiling,
	vk::ImageUsageFlags imageUsage, vk::MemoryPropertyFlags memoryProperties, vk::DeviceMemory& imageMemory)
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

	vk::Image image = nullptr;
	if (m_logicalDevice.createImage(&imageInfo, nullptr, &image) != vk::Result::eSuccess || !image)
		throw std::runtime_error("failed to create image for texture!");

	vk::MemoryRequirements imageMemRequirements = {};
	m_logicalDevice.getImageMemoryRequirements(image, &imageMemRequirements);

	vk::MemoryAllocateInfo imageAllocInfo {
		.allocationSize = imageMemRequirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(imageMemRequirements.memoryTypeBits, memoryProperties)
	};

	if (m_logicalDevice.allocateMemory(&imageAllocInfo, nullptr, &imageMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate image memory for texture!");

	m_logicalDevice.bindImageMemory(image, imageMemory, 0);

	LOG_SUCCESS("Successfully created image!");

	return image;
}

vk::CommandBuffer Graphics::BeginSingleTimeCommands()
{
	LOG_INFO("Creating and starting single time commands buffer!");

	vk::CommandBufferAllocateInfo bufferAllocInfo{
		.commandPool = m_commandPoolDraw,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	vk::CommandBuffer commandBuffer {};
	if (m_logicalDevice.allocateCommandBuffers(&bufferAllocInfo, &commandBuffer) != vk::Result::eSuccess)
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

	if (m_queueGraphics.submit(1, &submitInfo, nullptr) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit command buffer to graphics queue!");

	m_queueGraphics.waitIdle();

	m_logicalDevice.freeCommandBuffers(m_commandPoolDraw, 1, &commandBuffer);

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
	vk::PipelineStageFlags dstStage{};

	if (newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
	{
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

		if (HasStencilComponent(format))
			barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
		else
			barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	}

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else
		throw std::runtime_error("Unsupported layout transition!");

	commandBuffer.pipelineBarrier(srcStage, dstStage,
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

bool Graphics::HasStencilComponent(vk::Format format)
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void Graphics::CreateTextureImageView()
{
	m_textureImageView = CreateImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

vk::ImageView Graphics::CreateImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
	vk::ImageViewCreateInfo imageViewInfo {
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = vk::ImageSubresourceRange { aspectFlags, 0, 1, 0, 1 }
	};

	vk::ImageView imageView = nullptr;
	if (m_logicalDevice.createImageView(&imageViewInfo, nullptr, &imageView) != vk::Result::eSuccess)
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
		.anisotropyEnable = m_physicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = m_physicalDeviceProps.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.f,
		.maxLod = 0.f,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	if (m_logicalDevice.createSampler(&samplerInfo, nullptr, &m_textureSampler) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create texture sampler!");
}

void Graphics::CreateDepthResources()
{
	m_depthImage = CreateImage(m_swapChainInfo.imageExtent.width, m_swapChainInfo.imageExtent.height,
		m_depthImageFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, m_depthImageMemory);

	m_depthImageView = CreateImageView(m_depthImage, m_depthImageFormat, vk::ImageAspectFlagBits::eDepth);

	TransitionImageLayout(m_depthImage, m_depthImageFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void Graphics::CreateWindowSurface()
{
	LOG_INFO("Creating window surface...");

	VkSurfaceKHR pSurface = nullptr;
	if (glfwCreateWindowSurface(m_vulkanInstance, m_window, nullptr, &pSurface) != VK_SUCCESS)
		throw std::runtime_error("failed to create window surface!");
	m_windowSurface = pSurface;

	LOG_SUCCESS("Window surface created successfully!");
}

bool Graphics::IsDeviceSuitable(const vk::PhysicalDevice& device)
{
	LOG_INFO("Checking if device is suitable...");

	device.getProperties(&m_physicalDeviceProps);
	if (m_physicalDeviceProps.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
	{
		LOG_WARNING("Device is not a discrete GPU, skipping device...");
		return false;
	}

	// get max GPU samples for MSAA
	vk::SampleCountFlags sampleCountFlags = m_physicalDeviceProps.limits.framebufferColorSampleCounts & m_physicalDeviceProps.limits.framebufferDepthSampleCounts;
	if (sampleCountFlags & vk::SampleCountFlagBits::e64)
		m_maxMSAA = vk::SampleCountFlagBits::e64;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e32)
		m_maxMSAA = vk::SampleCountFlagBits::e32;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e16)
		m_maxMSAA = vk::SampleCountFlagBits::e16;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e8)
		m_maxMSAA = vk::SampleCountFlagBits::e8;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e4)
		m_maxMSAA = vk::SampleCountFlagBits::e4;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e2)
		m_maxMSAA = vk::SampleCountFlagBits::e2;

	device.getFeatures(&m_physicalDeviceFeatures);
	if (!m_physicalDeviceFeatures.geometryShader)
	{
		LOG_WARNING("Device does not support geometry shaders, skipping device...");
		return false;
	}

	m_physicalDeviceFeatures2.pNext = &m_physicalDeviceVK12Features;
	device.getFeatures2(&m_physicalDeviceFeatures2);

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

	m_physicalDeviceQueueFamiliesProps = std::vector<vk::QueueFamilyProperties>(queueFamilyCount);
	device.getQueueFamilyProperties(&queueFamilyCount, m_physicalDeviceQueueFamiliesProps.data());
	auto queueFamilyProperties = std::vector<std::vector<bool>>(queueFamilyCount);

	int i = 0;
	bool presentQueueFound = false, graphicsQueueFound = false, transferQueueFound = false;
	for (const auto& queueFamily : m_physicalDeviceQueueFamiliesProps)
	{
		std::vector<bool> queueFamiliesFound = { false, false, false }; // graphics, compute, present

		if (queueFamily.queueFlags & vk::QueueFlagBits::eCompute)
		{
			queueFamiliesFound[0] = true; // has graphics support
			graphicsQueueFound = true;
			m_physicalDeviceGraphicsIndex = i; 
		}

		if (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer)
		{
			queueFamiliesFound[1] = true; // has graphics support
			transferQueueFound = true;
			m_physicalDeviceTransferIndex = i;
		}

		vk::Bool32 windowPresentSupport = false;
		if (device.getSurfaceSupportKHR(i, m_windowSurface, &windowPresentSupport) != vk::Result::eSuccess)
			throw std::runtime_error("getSurfaceSupportKHR failed!");

		if (windowPresentSupport)
		{
			queueFamiliesFound[2] = true; // has present support
			presentQueueFound = true;
			m_physicalDevicePresentIndex = i;
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
			m_physicalDeviceGraphicsIndex = i;
			m_physicalDevicePresentIndex = i;
			m_physicalDeviceTransferIndex = i;
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
	m_physicalDeviceExtensions = std::vector<vk::ExtensionProperties>(extensionCount);
	if (device.enumerateDeviceExtensionProperties(nullptr, &extensionCount, m_physicalDeviceExtensions.data()) != vk::Result::eSuccess || !extensionCount)
	{
		LOG_WARNING("enumerateDeviceExtensionProperties failed!");
		return false;
	}

	std::set<std::string> requiredExtensions(m_requiredDeviceExtensionNames.begin(), m_requiredDeviceExtensionNames.end());
	for (const auto& extension : m_physicalDeviceExtensions) {
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
	if (device.getSurfaceFormatsKHR(m_windowSurface, &formatCount, nullptr) != vk::Result::eSuccess || !formatCount)
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_swapChainFormats = std::vector<vk::SurfaceFormatKHR>(formatCount);
	if (device.getSurfaceFormatsKHR(m_windowSurface, &formatCount, m_swapChainFormats.data()) != vk::Result::eSuccess || !formatCount)
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}

	// Get device swapchain present modes
	uint32_t presentModeCount;
	if (device.getSurfacePresentModesKHR(m_windowSurface, &presentModeCount, nullptr) != vk::Result::eSuccess || !presentModeCount)
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_swapChainPresentModes = std::vector<vk::PresentModeKHR>(presentModeCount);
	if (device.getSurfacePresentModesKHR(m_windowSurface, &presentModeCount, m_swapChainPresentModes.data()) != vk::Result::eSuccess || !presentModeCount)
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}

	// Get device swapchain surface capabilities for swap extent (resolution) and image count
	if (device.getSurfaceCapabilitiesKHR(m_windowSurface, &m_swapChainCapabilities) != vk::Result::eSuccess)
		throw std::runtime_error("getSurfaceCapabilitiesKHR failed!");

	// Get device's memory properties
	device.getMemoryProperties(&m_physicalDeviceMemoryProps);

	// Get format for depth buffer
	std::vector<vk::Format> depthFormats {
		vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint
	};

	std::optional<vk::Format> tempFormat;
	auto imageTiling = vk::ImageTiling::eOptimal;
	auto formatFeatures = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
	for (const auto& format : depthFormats)
	{
		vk::FormatProperties formatProps;
		device.getFormatProperties(format, &formatProps);

		if (imageTiling == vk::ImageTiling::eOptimal && (formatProps.optimalTilingFeatures & formatFeatures) == formatFeatures)
		{
			tempFormat = format;
			break;
		}
		else if (imageTiling == vk::ImageTiling::eLinear && (formatProps.linearTilingFeatures & formatFeatures) == formatFeatures)
		{
			tempFormat = format;
			break;
		}
	}

	if (!tempFormat.has_value())
		throw std::runtime_error("Failed to find supported format for depth image!");
	
	m_depthImageFormat = tempFormat.value();

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

	m_window = glfwCreateWindow(settings.uiWindowWidth, settings.uiWindowHeight, settings.sWindowName.c_str(), nullptr, nullptr);
	if (m_window == nullptr)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

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

	graphics->m_framebufferResized = true;
}

void Graphics::CleanupVulkan()
{
	LOG_INFO("Cleaning up Vulkan...");

	CleanupSwapChain();

	m_logicalDevice.destroySampler(m_textureSampler, nullptr);
	m_logicalDevice.destroyImageView(m_textureImageView, nullptr);

	m_logicalDevice.destroyImage(m_textureImage, nullptr);
	m_logicalDevice.freeMemory(m_textureImageMemory, nullptr);

	for (size_t i = 0; i < m_maxFramesInFlight; i++)
	{
		m_logicalDevice.destroyBuffer(m_uniformBuffers[i], nullptr);
		m_logicalDevice.freeMemory(m_uniformBuffersMemory[i], nullptr);
	}

	m_logicalDevice.destroyDescriptorPool(m_descriptorPool, nullptr);

	m_logicalDevice.destroyDescriptorSetLayout(m_descriptorSetLayout, nullptr);

	m_logicalDevice.destroyBuffer(m_indexBuffer, nullptr);
	m_logicalDevice.freeMemory(m_indexBufferMemory, nullptr);

	m_logicalDevice.destroyBuffer(m_vertexBuffer, nullptr);
	m_logicalDevice.freeMemory(m_vertexBufferMemory, nullptr);

	m_logicalDevice.destroyPipeline(m_pipelineGraphics, nullptr);
	m_logicalDevice.destroyPipelineLayout(m_pipelineLayout, nullptr);
	
	m_logicalDevice.destroyRenderPass(m_renderPass, nullptr);
	
	for (auto i = 0; i < m_renderFinishedSemaphores.size(); i++)
		m_logicalDevice.destroySemaphore(m_renderFinishedSemaphores[i], nullptr);

	for (auto i = 0; i < m_maxFramesInFlight; i++)
	{
		m_logicalDevice.destroyFence(m_frameInFlightFences[i], nullptr);
		m_logicalDevice.destroySemaphore(m_imageAvailableSemaphores[i], nullptr);
	}

	m_logicalDevice.destroyCommandPool(m_commandPoolDraw, nullptr);
	m_logicalDevice.destroyCommandPool(m_commandPoolTransfer, nullptr);

	m_logicalDevice.destroy(nullptr);

#ifdef _DEBUG
	CleanupDebugger();
#endif // _DEBUG

	m_vulkanInstance.destroySurfaceKHR(m_windowSurface, nullptr);
	m_vulkanInstance.destroy(nullptr);

	glfwDestroyWindow(m_window);
	glfwTerminate();

	LOG_SUCCESS("Vulkan cleaned up successfully!");
}

void Graphics::InitializeDebugger()
{
	LOG_INFO("Initializing Vulkan debugger...");

	m_debugMessenger = m_vulkanInstance.createDebugUtilsMessengerEXT(m_debugCreateInfo);

	//if (m_pVulkanInstance.createDebugUtilsMessengerEXT(&m_sDebugCreateInfo, nullptr, &m_pDebugMessenger) != vk::Result::eSuccess)
	//	throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed!");

	LOG_SUCCESS("Vulkan debugger initialized successfully!");
}

void Graphics::CleanupDebugger()
{
	LOG_INFO("Cleaning up Vulkan debugger...");

	m_vulkanInstance.destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr);

	LOG_SUCCESS("Vulkan debugger cleaned up successfully!");
}

bool Graphics::IsDebugSupported()
{
	LOG_INFO("Checking if Vulkan debugging is supported...");

	for (const auto& layerName : m_requiredValidationLayerNames)
		for (const auto& layer : m_vulkanLayerProps)
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