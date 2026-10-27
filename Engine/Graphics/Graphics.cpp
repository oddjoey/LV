#include "Graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

CLASS_SINGLETON_SOURCE(Graphics)

Graphics::Graphics()
{
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
	m_dlss.Initialize(m_vulkanInstance, m_physicalDevice, m_logicalDevice);

	CreateSwapChain();
	CreateImageViews();

	CreateRenderPass();

	CreateCommandPools();
	CreateMainCommandBuffers();

	CreateMSAAResources();
}

Graphics::~Graphics()
{
	
	
}

void Graphics::CreateVulkanInstance() 
{
	LOG_INFO("Creating Vulkan instance...");

	applicationInfo = {
		.pApplicationName = "Lordi",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Lordi Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_4
	};

	vk::InstanceCreateInfo createInfo {
		.pApplicationInfo = &applicationInfo,
		.enabledExtensionCount = 0,
	};

	uint32_t requiredGLFWExtensions = 0;
	const char** ppExtensions = glfwGetRequiredInstanceExtensions(&requiredGLFWExtensions);
	if (!ppExtensions)
		throw std::runtime_error("Required GLFW extensions not found!");

	std::vector<const char*> requiredExtensionNames(ppExtensions, ppExtensions + requiredGLFWExtensions);

	// DLSS (NGX) needs a few more, where the driver has them.
	std::vector<std::string> dlssExtensions = DLSS::InstanceExtensions();
	for (const auto& name : dlssExtensions)
	{
		bool supported = std::any_of(m_vulkanExtensionProps.begin(), m_vulkanExtensionProps.end(),
			[&](const vk::ExtensionProperties& extension) { return name == extension.extensionName.data(); });
		bool requested = std::any_of(requiredExtensionNames.begin(), requiredExtensionNames.end(),
			[&](const char* requestedName) { return name == requestedName; });
		if (supported && !requested)
			requiredExtensionNames.push_back(name.c_str());
	}

#ifdef _DEBUG
	requiredExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	createInfo.enabledLayerCount = static_cast<uint32_t>(m_requiredValidationLayerNames.size());
	createInfo.ppEnabledLayerNames = m_requiredValidationLayerNames.data();

	m_debugCreateInfo.sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT;
	m_debugCreateInfo.messageSeverity = 
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	m_debugCreateInfo.messageType = 
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	m_debugCreateInfo.pfnUserCallback = VulkanDebugCallback;

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
	deviceFeatures.depthBounds = vk::True;
	deviceFeatures.samplerAnisotropy = vk::True;
	deviceFeatures.sampleRateShading = vk::True;
	// DLSS's scene pass blends glass into the colour target but leaves its motion vectors alone.
	deviceFeatures.independentBlend = m_physicalDeviceFeatures.independentBlend;
	m_depthClampSupported = m_physicalDeviceFeatures.depthClamp == vk::True;
	deviceFeatures.depthClamp = m_depthClampSupported ? vk::True : vk::False;

	vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexFeatures {
		.shaderSampledImageArrayNonUniformIndexing = vk::True,
		.descriptorBindingVariableDescriptorCount = vk::True,
		.runtimeDescriptorArray = vk::True
	};

	// DLSS (NGX) device extensions, where the GPU has them. Without them DLSS just reports itself
	// unavailable.
	std::vector<const char*> deviceExtensionNames = m_requiredDeviceExtensionNames;
	std::vector<std::string> dlssExtensions = DLSS::DeviceExtensions(m_vulkanInstance, m_physicalDevice);
	bool bufferDeviceAddress = false;
	for (const auto& name : dlssExtensions)
	{
		bool supported = std::any_of(m_physicalDeviceExtensions.begin(), m_physicalDeviceExtensions.end(),
			[&](const vk::ExtensionProperties& extension) { return name == extension.extensionName.data(); });
		bool requested = std::any_of(deviceExtensionNames.begin(), deviceExtensionNames.end(),
			[&](const char* requestedName) { return name == requestedName; });
		if (supported && !requested)
		{
			deviceExtensionNames.push_back(name.c_str());
			LOG_DEBUG("Enabling device extension for DLSS: " + name);
		}
		if (supported && (name == VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME || name == VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
			bufferDeviceAddress = true;
	}
	vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures {
		.bufferDeviceAddress = vk::True
	};
	if (bufferDeviceAddress)
		descriptorIndexFeatures.pNext = &bufferDeviceAddressFeatures;

	vk::DeviceCreateInfo deviceCreateInfo {
		.pNext = &descriptorIndexFeatures,
		.flags = {},
		.queueCreateInfoCount = static_cast<uint32_t>(deviceQueuesInfo.size()),
		.pQueueCreateInfos = deviceQueuesInfo.data(),
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size()),
		.ppEnabledExtensionNames = deviceExtensionNames.data(),
		.pEnabledFeatures = &deviceFeatures,
	};

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

	// The window may have changed size or display mode since the device was picked.
	if (m_physicalDevice.getSurfaceCapabilitiesKHR(m_windowSurface, &m_windowSurfaceCapabilities) != vk::Result::eSuccess)
		throw std::runtime_error("getSurfaceCapabilitiesKHR failed!");

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
			m_swapChainRequestedPresentMode = vk::PresentModeKHR::eImmediate; // No VSync
			LOG_DEBUG("VSyncOptions::NONE / VK_PRESENT_MODE_IMMEDIATE_KHR");
			break;
		case VSyncOptions::ENABLED:
			m_swapChainRequestedPresentMode = vk::PresentModeKHR::eFifo; // VSync
			LOG_DEBUG("VSyncOptions::ENABLED / VK_PRESENT_MODE_FIFO_KHR");
			break;
		case VSyncOptions::RELAXED:
			m_swapChainRequestedPresentMode = vk::PresentModeKHR::eFifoRelaxed; // Relaxed VSync
			LOG_DEBUG("VSyncOptions::RELAXED / VK_PRESENT_MODE_FIFO_RELAXED_KHR");
			break;
		case VSyncOptions::MAILBOX:
			m_swapChainRequestedPresentMode = vk::PresentModeKHR::eMailbox; // Mailbox VSync
			LOG_DEBUG("VSyncOptions::MAILBOX / VK_PRESENT_MODE_MAILBOX_KHR");
			break;
	}

	m_swapChainPresentMode = vk::PresentModeKHR::eFifo;
	bool presentFound = false;
	for (const auto& presentMode : m_swapChainPresentModes)
		if (presentMode == m_swapChainRequestedPresentMode)
		{
			presentFound = true;
			m_swapChainPresentMode = presentMode;
			break;
		}
	if (!presentFound)
		LOG_WARNING("Requested present mode not found on device, using defaults (FIFO / VYSNC) as its guaranteed...");

	vk::Extent2D swapChainExtent = {};
	if (m_windowSurfaceCapabilities.currentExtent.width == UINT32_MAX)
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), m_windowSurfaceCapabilities.minImageExtent.width, m_windowSurfaceCapabilities.maxImageExtent.width);
		swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), m_windowSurfaceCapabilities.minImageExtent.height, m_windowSurfaceCapabilities.maxImageExtent.height);
	}
	else
		swapChainExtent = m_windowSurfaceCapabilities.currentExtent;

	m_swapChainImageCount = m_windowSurfaceCapabilities.minImageCount + 1;
	if (m_windowSurfaceCapabilities.maxImageCount > 0 && m_swapChainImageCount > m_windowSurfaceCapabilities.maxImageCount)
	{
		LOG_WARNING("Swap chain image count exceeds maximum, clamping to maximum...");
		m_swapChainImageCount = m_windowSurfaceCapabilities.maxImageCount;
	}

	m_swapChainInfo = {
		.flags = {},
		.surface = m_windowSurface,
		.minImageCount = m_swapChainImageCount,
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

	m_swapChainInfo.preTransform = m_windowSurfaceCapabilities.currentTransform;
	m_swapChainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	// presentation mode (VSync)
	m_swapChainInfo.presentMode = m_swapChainPresentMode;
	// clipped swap chain images provide the best performance compared to non-clipped images
	// clipped images are not presented if they are obscured by other windows
	m_swapChainInfo.clipped = VK_TRUE;
	// if recreating swapchain, provide previous one here
	// TO-DO: Implement swap chain recreation logic
	m_swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

	if (m_logicalDevice.createSwapchainKHR(&m_swapChainInfo, nullptr, &m_swapChain) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create swapchain!");

	// Get swap chain images
	if (m_logicalDevice.getSwapchainImagesKHR(m_swapChain, &m_swapChainImageCount, nullptr) != vk::Result::eSuccess || !m_swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	m_swapChainImages = std::vector<vk::Image>(m_swapChainImageCount);
	if (m_logicalDevice.getSwapchainImagesKHR(m_swapChain, &m_swapChainImageCount, m_swapChainImages.data()) != vk::Result::eSuccess || !m_swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");

	LOG_SUCCESS("Swapchain created successfully!");
}

void Graphics::CreateImageViews()
{
	LOG_INFO("Creating image views for swapchain images...");

	m_swapChainImageViews = std::vector<vk::ImageView>(m_swapChainImages.size());
	for (auto i = 0; i < m_swapChainImages.size(); i++)
		m_swapChainImageViews[i] = CreateImageView(m_swapChainImages[i], m_swapChainFormat.format, vk::ImageAspectFlagBits::eColor, 1);

	LOG_SUCCESS("Image views for swapchain images created successfully!");
}

void Graphics::CreateRenderPass()
{
	LOG_INFO("Creating render pass...");

	vk::AttachmentDescription colorAttachment = {
		.flags = {},
		.format = m_swapChainInfo.imageFormat,
		.samples = m_MSAALevel,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = m_MSAALevel != vk::SampleCountFlagBits::e1 ? vk::ImageLayout::eColorAttachmentOptimal : vk::ImageLayout::ePresentSrcKHR
	};
	vk::AttachmentReference colorAttachmentRef = {
		.attachment = 0,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};

	vk::AttachmentDescription depthAttachment = {
		.flags = {},
		.format = m_depthImageFormat,
		.samples = m_MSAALevel,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::AttachmentReference depthAttachmentRef = {
		.attachment = 1,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};

	vk::AttachmentDescription colorAttachmentResolve = {
		.flags = {},
		.format = m_swapChainInfo.imageFormat,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eDontCare,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::ePresentSrcKHR
	};
	vk::AttachmentReference colorAttachmentResolveRef = {
		.attachment = 2,
		.layout = vk::ImageLayout::eColorAttachmentOptimal
	};

	std::vector<vk::AttachmentDescription> attachments = { colorAttachment, depthAttachment };
	if (m_MSAALevel != vk::SampleCountFlagBits::e1)
		attachments.push_back(colorAttachmentResolve);

	vk::SubpassDescription sceneSubpass = {
		.flags = {},
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = (m_MSAALevel != vk::SampleCountFlagBits::e1 ? 
			&colorAttachmentResolveRef : nullptr),
		.pDepthStencilAttachment = &depthAttachmentRef,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	vk::SubpassDescription imguiSubpass = {
		.flags = {},
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentRef,
		.pResolveAttachments = (m_MSAALevel != vk::SampleCountFlagBits::e1 ?
			&colorAttachmentResolveRef : nullptr),
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	std::vector<vk::SubpassDescription> subpasses = { sceneSubpass, imguiSubpass };

	vk::SubpassDependency dependency {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
	};

	vk::RenderPassCreateInfo renderPassInfo {
		.flags = {},
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = static_cast<uint32_t>(subpasses.size()),
		.pSubpasses = subpasses.data(),
		.dependencyCount = 1,
		.pDependencies = &dependency
	};

	if (m_logicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_renderPass) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create render pass!");

	LOG_SUCCESS("Render pass created successfully!");
}

void Graphics::CreateGraphicsPipeline()
{
	LOG_INFO("Creating graphics pipeline...");

	std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = { m_dynamicUBODescriptorSetLayout, m_samplerDescriptorSetLayout, m_sceneDescriptorSetLayout };

	vk::PushConstantRange objectConstants {
		.stageFlags = vk::ShaderStageFlagBits::eFragment,
		.offset = 0,
		.size = sizeof(ObjectPushConstants)
	};

	std::vector<vk::PushConstantRange> pushConstants = { objectConstants };

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.flags = {},
		.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
		.pSetLayouts = descriptorSetLayouts.data(),
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
		.pPushConstantRanges = pushConstants.data()
	};
	if (m_logicalDevice.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create pipeline layout!");

	CreateScenePipelines(m_renderPass, m_MSAALevel, "Engine/Graphics/Shaders/Compiled/vert.spv", "Engine/Graphics/Shaders/Compiled/frag.spv",
		1, m_pipelineGraphics, m_pipelineTransparent);

	LOG_SUCCESS("Graphics pipeline created successfully!");
}

vk::ShaderModule Graphics::LoadShaderModule(const char* path)
{
	// Relative to Lordi's folder, which a game with Lordi as a submodule sets in its settings.
	auto shaderCode = Utility::ReadFile(Settings::Get()->GetGraphicsSettings().sEngineRoot + path);
	vk::ShaderModuleCreateInfo shaderInfo {
		.flags = {},
		.codeSize = shaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
	};
	vk::ShaderModule shaderModule = {};
	if (m_logicalDevice.createShaderModule(&shaderInfo, nullptr, &shaderModule) != vk::Result::eSuccess)
		throw std::runtime_error(std::string("failed to create shader module for ") + path + "!");
	return shaderModule;
}

void Graphics::CreateScenePipelines(vk::RenderPass renderPass, vk::SampleCountFlagBits samples, const char* vertexPath, const char* fragmentPath,
	uint32_t colorAttachments, vk::Pipeline& opaque, vk::Pipeline& transparent)
{
	LOG_DEBUG("Creating shader modules for vertex and fragment shaders...");

	auto graphicsSettings = Settings::Get()->GetGraphicsSettings();

	// Load shader byte code into memory
	m_shaderModuleVertex = LoadShaderModule(vertexPath);
	m_shaderModuleFragment = LoadShaderModule(fragmentPath);

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
	vk::VertexInputAttributeDescription vertexNormalDesc {
		.location = 3,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, normal)
	};
	std::vector<vk::VertexInputAttributeDescription> vertexDescriptions {
		vertexPositionDesc,
		vertexColorDesc,
		vertexTexDesc,
		vertexNormalDesc
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
		.rasterizationSamples = samples,
		.sampleShadingEnable = samples != vk::SampleCountFlagBits::e1 ? vk::True : vk::False,
		.minSampleShading = 0.2f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE
	};

	vk::PipelineDepthStencilStateCreateInfo depthStencilInfo {
		.flags = {},
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
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
	// Any further attachment (motion vectors) is written as is.
	vk::PipelineColorBlendAttachmentState extraAttachment {
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments(colorAttachments, extraAttachment);
	colorBlendAttachments[0] = colorBlendAttachment;
	vk::PipelineColorBlendStateCreateInfo colorBlendInfo {
		.flags = {},
		.logicOpEnable = graphicsSettings.eColorBlending == ColorBlendingOptions::BITWISE ? VK_TRUE : VK_FALSE,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
		.pAttachments = colorBlendAttachments.data(),
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
		.renderPass = renderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1
	};

	if (m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &opaque) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create graphics pipeline!");

	// The same pipeline blended over what's behind it, for glass: tested against depth but not
	// writing it, drawn back to front after everything solid. It leaves the motion vectors of
	// whatever is behind the glass alone.
	colorBlendAttachments[0].blendEnable = vk::True;
	colorBlendAttachments[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachments[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachments[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachments[0].dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	for (uint32_t i = 1; i < colorAttachments; i++)
		colorBlendAttachments[i].colorWriteMask = {};
	colorBlendInfo.logicOpEnable = vk::False;
	depthStencilInfo.depthWriteEnable = vk::False;
	rasterizerInfo.cullMode = vk::CullModeFlagBits::eNone;
	if (m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &transparent) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the transparent pipeline!");

	// cleanup shader modules
	m_logicalDevice.destroyShaderModule(m_shaderModuleFragment, nullptr);
	m_logicalDevice.destroyShaderModule(m_shaderModuleVertex, nullptr);
}

void Graphics::CreateFrameBuffers()
{
	LOG_INFO("Creating frame buffers for swapchain image views...");

	m_frameBuffers = std::vector<vk::Framebuffer>(m_swapChainImageViews.size());

	for (auto i = 0; i < m_swapChainImageViews.size(); i++)
	{
		std::vector<vk::ImageView> attachments = {};
		if (m_MSAALevel != vk::SampleCountFlagBits::e1)
			attachments = { m_MSAAImageView, m_depthImageView, m_swapChainImageViews[i] };
		else
			attachments = { m_swapChainImageViews[i], m_depthImageView };

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

void Graphics::CreateMainCommandBuffers()
{
	m_commandBuffers = CreateCommandBuffers(m_maxFramesInFlight);
}

void Graphics::CreateSyncObjects()
{
	LOG_INFO("Creating synchronization objects...");

	vk::SemaphoreCreateInfo semaphoreInfo {
		.flags = {}
	};

	m_imageAvailableSemaphores = std::vector<vk::Semaphore>(m_maxFramesInFlight, nullptr);
	m_frameInFlightFences = std::vector<vk::Fence>(m_maxFramesInFlight, nullptr);

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

	m_mainRenderFinishedSemaphores = std::vector<vk::Semaphore>(m_swapChainImageCount, nullptr);
	m_imguiRenderFinishedSemaphores = std::vector<vk::Semaphore>(m_swapChainImageCount, nullptr);

	for (uint32_t i = 0; i < m_swapChainImageCount; i++)
	{
		if (m_logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_mainRenderFinishedSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create render finished semaphore for swap chain image!");
		if (m_logicalDevice.createSemaphore(&semaphoreInfo, nullptr, &m_imguiRenderFinishedSemaphores[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create render finished semaphore for swap chain image!");
	}

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
	CreateMSAAResources();
	CreateDepthResources();
	CreateFrameBuffers();
	// DLSS's targets follow the window size.
	if (m_dlssMode != DLSSMode::OFF)
		CreateUpscaleTargets();

	LOG_SUCCESS("Swapchain recreated successfully!");
}

void Graphics::CleanupSwapChain()
{
	LOG_INFO("Cleaning up swapchain...");

	m_logicalDevice.destroyImageView(m_MSAAImageView, nullptr);
	m_logicalDevice.destroyImage(m_MSAAImage, nullptr);
	m_logicalDevice.freeMemory(m_MSAAImageMemory, nullptr);

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

void Graphics::CopyDataToBuffer(void* data, vk::DeviceSize size, vk::Buffer& buffer)
{
	vk::DeviceMemory stagingBufferMemory;
	vk::Buffer stagingBuffer = CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBufferMemory);

	void* mappedData = nullptr;
	if (m_logicalDevice.mapMemory(stagingBufferMemory, 0, size, {}, &mappedData) != vk::Result::eSuccess)
		throw std::runtime_error("failed to map staging memory for vertex buffer!");
	memcpy(mappedData, data, static_cast<size_t>(size));
	m_logicalDevice.unmapMemory(stagingBufferMemory);

	CopyBufferToBuffer(stagingBuffer, buffer, size);
	m_logicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_logicalDevice.freeMemory(stagingBufferMemory, nullptr);
}

vk::Buffer Graphics::CreateBuffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags bufferUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, vk::DeviceMemory& deviceMemory, bool cleanup)
{
	LOG_INFO("Creating buffer...");

	vk::BufferCreateInfo bufferInfo {
		.size = bufferSize,
		.usage = bufferUsageFlags,
		.sharingMode = vk::SharingMode::eExclusive
	};

	vk::Buffer buffer;
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
	if (cleanup)
	{
		m_manageMemories.push_back(deviceMemory);
		m_manageBuffers.push_back(buffer);
	}

	return buffer;
}

void Graphics::CopyBufferToBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize)
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

void Graphics::CreateDescriptorSetLayout(const uint32_t& numOfTextures)
{
	LOG_INFO("Creating descriptor set layout...");

	vk::DescriptorSetLayoutBinding dynamicUBO {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.pImmutableSamplers = nullptr
	};

	vk::DescriptorSetLayoutBinding combinedSampler {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = numOfTextures,
		.stageFlags = vk::ShaderStageFlagBits::eFragment,
		.pImmutableSamplers = nullptr
	};

	std::vector<vk::DescriptorBindingFlags> bindingFlags(1);
	bindingFlags[bindingFlags.size() - 1] = vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

	vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo {
		.bindingCount = static_cast<uint32_t>(bindingFlags.size()),
		.pBindingFlags = bindingFlags.data()
	};

	vk::DescriptorSetLayoutCreateInfo dynamicUBOLayoutCreateInfo {
		.bindingCount = 1,
		.pBindings = &dynamicUBO
	};

	vk::DescriptorSetLayoutCreateInfo samplerLayoutCreateInfo {
		.pNext = &flagsCreateInfo,
		.bindingCount = 1,
		.pBindings = &combinedSampler
	};

	if (m_logicalDevice.createDescriptorSetLayout(&dynamicUBOLayoutCreateInfo, nullptr, &m_dynamicUBODescriptorSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor set layout for UBO!");

	if (m_logicalDevice.createDescriptorSetLayout(&samplerLayoutCreateInfo, nullptr, &m_samplerDescriptorSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor set layout for sampler!");

	LOG_SUCCESS("Successfully created descriptor set layout!");
}

void Graphics::CreateDynamicUniformBuffers()
{
	LOG_INFO("Creating uniform buffers...");

	vk::DeviceSize minAlignment = m_physicalDeviceProps.limits.minUniformBufferOffsetAlignment - 1;
	m_alignedUBOSize = (sizeof(UniformBuffer) + minAlignment) & ~minAlignment;

	m_dynamicUniformBufferObjectCount = MAX_OBJECTS;
	m_dynamicUniformBufferSize = m_alignedUBOSize * m_dynamicUniformBufferObjectCount;

	m_dynamicUniformBuffers = std::vector<vk::Buffer>(m_maxFramesInFlight);
	m_dynamicUniformBuffersMemory = std::vector<vk::DeviceMemory>(m_maxFramesInFlight);
	m_dynamicUniformBuffersMapped = std::vector<void*>(m_maxFramesInFlight);

	for (size_t i = 0; i < m_maxFramesInFlight; i++)
	{
		m_dynamicUniformBuffers[i] = CreateBuffer(m_dynamicUniformBufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_dynamicUniformBuffersMemory[i], true);

		if (m_logicalDevice.mapMemory(m_dynamicUniformBuffersMemory[i], 0, m_dynamicUniformBufferSize, {}, &m_dynamicUniformBuffersMapped[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to map user memory to uniform buffer!");
	}

	LOG_SUCCESS("Successfully created uniform buffers!");
}

void Graphics::UpdateUniformBuffer(uint32_t imageIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBuffer ubo {};
	ubo.model = glm::rotate(glm::mat4(1.f), duration * glm::radians(-90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.proj = glm::perspective(glm::radians(45.f),
		static_cast<float>(m_windowSurfaceCapabilities.currentExtent.width) / static_cast<float>(m_windowSurfaceCapabilities.currentExtent.width),
		0.1f, 10.f);

	ubo.proj[1][1] *= -1;

	memcpy(m_dynamicUniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
}

void Graphics::CreateDescriptorPool(const uint32_t& numOfTextures)
{
	vk::DescriptorPoolSize dynamicUBOPoolSize {
		.type = vk::DescriptorType::eUniformBufferDynamic,
		.descriptorCount = static_cast<uint32_t>(m_maxFramesInFlight)
	};

	// Textures, plus the sun and lamp shadow maps in each scene set.
	vk::DescriptorPoolSize texturePoolSize {
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = numOfTextures + static_cast<uint32_t>(m_maxFramesInFlight) * 2
	};

	vk::DescriptorPoolSize sceneUBOPoolSize {
		.type = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = static_cast<uint32_t>(m_maxFramesInFlight)
	};

	std::vector<vk::DescriptorPoolSize> poolSizes { dynamicUBOPoolSize, texturePoolSize, sceneUBOPoolSize };

	vk::DescriptorPoolCreateInfo poolInfo {
		.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
		.maxSets = static_cast<uint32_t>(m_maxFramesInFlight * 2 + numOfTextures),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	if (m_logicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_graphicsDescriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor pool!");
}

void Graphics::CreateDescriptorSets(const std::vector<vk::DescriptorImageInfo>& textureDescriptors)
{
	std::vector<vk::DescriptorSetLayout> dynamicUBODescriptorSetLayouts(m_maxFramesInFlight, m_dynamicUBODescriptorSetLayout);
	vk::DescriptorSetAllocateInfo uboDescriptorSetAllocInfo {
		.descriptorPool = m_graphicsDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(m_maxFramesInFlight),
		.pSetLayouts = dynamicUBODescriptorSetLayouts.data()
	};

	m_dynamicUBODescriptorSets = std::vector<vk::DescriptorSet>(m_maxFramesInFlight);
	if (m_logicalDevice.allocateDescriptorSets(&uboDescriptorSetAllocInfo, m_dynamicUBODescriptorSets.data()) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	for (size_t i = 0; i < m_maxFramesInFlight; i++)
	{
		vk::DescriptorBufferInfo dynamicUBOInfo {
			.buffer = m_dynamicUniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBuffer)
		};

		vk::WriteDescriptorSet dynamicUBODescWrite {
			.dstSet = m_dynamicUBODescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
			.pImageInfo = nullptr,
			.pBufferInfo = &dynamicUBOInfo,
			.pTexelBufferView = nullptr
		};

		std::vector<vk::WriteDescriptorSet> descriptorWrites { dynamicUBODescWrite };
		
		m_logicalDevice.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	std::vector<uint32_t> varDescCounts = { static_cast<uint32_t>(textureDescriptors.size()) };
	vk::DescriptorSetVariableDescriptorCountAllocateInfo varDescCountAllocInfo {
		.descriptorSetCount = static_cast<uint32_t>(varDescCounts.size()),
		.pDescriptorCounts = varDescCounts.data(),
	};

	vk::DescriptorSetAllocateInfo samplerDescriptorSetAllocInfo {
		.pNext = &varDescCountAllocInfo,
		.descriptorPool = m_graphicsDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_samplerDescriptorSetLayout
	};

	if (m_logicalDevice.allocateDescriptorSets(&samplerDescriptorSetAllocInfo, &m_samplerDescriptorSet) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	vk::WriteDescriptorSet imageDescWrite {
		.dstSet = m_samplerDescriptorSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = varDescCounts[0],
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.pImageInfo = textureDescriptors.data(),
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr
	};

	m_logicalDevice.updateDescriptorSets(1, &imageDescWrite, 0, nullptr);
}

vk::Image Graphics::CreateImage(uint32_t imageWidth, uint32_t imageHeight, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount,
	vk::Format imageFormat, vk::ImageTiling imageTiling, vk::ImageUsageFlags imageUsage, vk::MemoryPropertyFlags memoryProperties, 
	vk::DeviceMemory& deviceMemory, bool cleanup)
{
	LOG_INFO("Creating image...");

	vk::ImageCreateInfo imageInfo {
		.flags = {},
		.imageType = vk::ImageType::e2D,
		.format = imageFormat,
		.extent = vk::Extent3D { static_cast<uint32_t>(imageWidth), static_cast<uint32_t>(imageHeight), 1 },
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = sampleCount,
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

	if (m_logicalDevice.allocateMemory(&imageAllocInfo, nullptr, &deviceMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate image memory for texture!");

	m_logicalDevice.bindImageMemory(image, deviceMemory, 0);

	LOG_SUCCESS("Successfully created image!");
	if (cleanup)
	{
		m_manageMemories.push_back(deviceMemory);
		m_manageImages.push_back(image);
	}
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

void Graphics::TransitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
	uint32_t mipLevels, const vk::CommandBuffer* inCommandBuffer)
{
	vk::CommandBuffer commandBuffer = {};

	if (inCommandBuffer != nullptr)
		commandBuffer = *inCommandBuffer;
	else
		commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier {
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eNone,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1 }
	};

	vk::PipelineStageFlags srcStage{};
	vk::PipelineStageFlags dstStage{};

	if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

		if (HasStencilComponent(format))
			barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
	}

	if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eGeneral)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
	}
	else if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eColorAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
		newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		srcStage = vk::PipelineStageFlagBits::eFragmentShader;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		barrier.dstAccessMask = vk::AccessFlagBits::eNone;

		srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
		newLayout == vk::ImageLayout::eColorAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		srcStage = vk::PipelineStageFlagBits::eFragmentShader;
		dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	}
	else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal &&
		newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;

		srcStage = vk::PipelineStageFlagBits::eFragmentShader;
		dstStage = vk::PipelineStageFlagBits::eLateFragmentTests;
	}
	else if (oldLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal &&
		newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
			barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			srcStage = vk::PipelineStageFlagBits::eLateFragmentTests;
			dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
		newLayout == vk::ImageLayout::ePresentSrcKHR)
	{
			barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eNone;

			srcStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dstStage = vk::PipelineStageFlagBits::eNone;
	}
	else
		throw std::runtime_error("Unsupported layout transition!");

	commandBuffer.pipelineBarrier(srcStage, dstStage,
		{}, 0, nullptr, 0, nullptr, 1, &barrier);

	EndSingleTimeCommands(commandBuffer);
}

void Graphics::CopyDataToImage(void* data, vk::DeviceSize size, vk::Image& image, uint32_t imageWidth, uint32_t imageHeight)
{
	vk::DeviceMemory stagingBufferMemory;
	vk::Buffer stagingBuffer = CreateBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBufferMemory);

	CopyDataToBuffer(data, size, stagingBuffer);
	CopyBufferToImage(stagingBuffer, image, imageWidth, imageHeight);

	m_logicalDevice.destroyBuffer(stagingBuffer, nullptr);
	m_logicalDevice.freeMemory(stagingBufferMemory, nullptr);
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

vk::ImageView Graphics::CreateImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels, bool cleanup)
{
	vk::ImageViewCreateInfo imageViewInfo {
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = format,
		.subresourceRange = vk::ImageSubresourceRange { aspectFlags, 0, mipLevels, 0, 1 }
	};

	vk::ImageView imageView = nullptr;
	if (m_logicalDevice.createImageView(&imageViewInfo, nullptr, &imageView) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create texture image view!");
	if (cleanup)
		m_manageImageViews.push_back(imageView);
	return imageView;
}

vk::Sampler Graphics::CreateSampler(bool cleanup)
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
		.maxLod = vk::LodClampNone,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	vk::Sampler sampler = {};
	if (m_logicalDevice.createSampler(&samplerInfo, nullptr, &sampler) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create texture sampler!");
	if (cleanup)
		m_manageSamplers.push_back(sampler);
	return sampler;
}

void Graphics::GenerateMipmaps(const vk::Image& image, const int32_t& width, const int32_t& height, const uint32_t& mipLevels)
{
	auto commandBuffer = BeginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier {
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = image,
		.subresourceRange = vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	};

	int32_t mipWidth = width, mipHeight = height;
	for (uint32_t i = 1; i < mipLevels; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr,
			0, nullptr, 1, &barrier);

		vk::ImageBlit blit {
			.srcSubresource = vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, i - 1, 0, 1 },
			.srcOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D {0, 0, 0}, vk::Offset3D {mipWidth, mipHeight, 1 } },
			.dstSubresource = vk::ImageSubresourceLayers { vk::ImageAspectFlagBits::eColor, i, 0, 1 },
			.dstOffsets = std::array<vk::Offset3D, 2> { vk::Offset3D {0, 0, 0},
				vk::Offset3D {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}}
		};

		commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, 1,
			&blit, vk::Filter::eLinear);

		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr,
			0, nullptr, 1, &barrier);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr,
		0, nullptr, 1, &barrier);

	EndSingleTimeCommands(commandBuffer);
}

std::vector<vk::CommandBuffer> Graphics::CreateCommandBuffers(const uint32_t& count)
{
	LOG_INFO("Creating command buffer...");

	std::vector<vk::CommandBuffer> commandBuffers(count);

	vk::CommandBufferAllocateInfo allocateInfo = {
		.commandPool = m_commandPoolDraw,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = count
	};

	if (m_logicalDevice.allocateCommandBuffers(&allocateInfo, commandBuffers.data()) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate command buffers!");

	LOG_SUCCESS("Created command buffer successfully!");
	return commandBuffers;
}

void Graphics::BeginCommandBuffer(vk::CommandBuffer& commandBuffer, vk::CommandBufferUsageFlags usageFlags)
{
	vk::CommandBufferBeginInfo beginInfo = {
		.flags = usageFlags,
		.pInheritanceInfo = nullptr
	};

	if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to begin command buffer!");
}

void Graphics::CreateDepthResources()
{
	m_depthImage = CreateImage(m_swapChainInfo.imageExtent.width, m_swapChainInfo.imageExtent.height, 1, m_MSAALevel,
		m_depthImageFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, m_depthImageMemory);

	m_depthImageView = CreateImageView(m_depthImage, m_depthImageFormat, vk::ImageAspectFlagBits::eDepth, 1);

	TransitionImageLayout(m_depthImage, m_depthImageFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
}

void Graphics::CreateMSAAResources()
{
	m_MSAAImage = CreateImage(m_windowSurfaceCapabilities.currentExtent.width, m_windowSurfaceCapabilities.currentExtent.height, 1,
		m_MSAALevel, m_swapChainInfo.imageFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal, m_MSAAImageMemory);

	m_MSAAImageView = CreateImageView(m_MSAAImage, m_swapChainInfo.imageFormat, vk::ImageAspectFlagBits::eColor, 1);
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
		m_MSAAMax = vk::SampleCountFlagBits::e64;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e32)
		m_MSAAMax = vk::SampleCountFlagBits::e32;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e16)
		m_MSAAMax = vk::SampleCountFlagBits::e16;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e8)
		m_MSAAMax = vk::SampleCountFlagBits::e8;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e4)
		m_MSAAMax = vk::SampleCountFlagBits::e4;
	else if (sampleCountFlags & vk::SampleCountFlagBits::e2)
		m_MSAAMax = vk::SampleCountFlagBits::e2;

	auto settings = Settings::Get()->GetGraphicsSettings();
	switch (settings.eMSAA)
	{
	case MSAAOptions::X1:
		m_MSAALevel = vk::SampleCountFlagBits::e1; // no multisampling
		LOG_DEBUG("MSAAOptions::X1 / VK_SAMPLE_COUNT_1_BIT");
		break;
	case MSAAOptions::X2:
		m_MSAALevel = vk::SampleCountFlagBits::e2; // 2x multisampling
		LOG_DEBUG("MSAAOptions::X2 / VK_SAMPLE_COUNT_2_BIT");
		break;
	case MSAAOptions::X4:
		m_MSAALevel = vk::SampleCountFlagBits::e4; // 4x multisampling
		LOG_DEBUG("MSAAOptions::X4 / VK_SAMPLE_COUNT_4_BIT");
		break;
	case MSAAOptions::X8:
		m_MSAALevel = vk::SampleCountFlagBits::e8; // 8x multisampling
		LOG_DEBUG("MSAAOptions::X8 / VK_SAMPLE_COUNT_8_BIT");
		break;
	case MSAAOptions::X16:
		m_MSAALevel = vk::SampleCountFlagBits::e16; // 16x multisampling
		LOG_DEBUG("MSAAOptions::X16 / VK_SAMPLE_COUNT_16_BIT");
		break;
	case MSAAOptions::X32:
		m_MSAALevel = vk::SampleCountFlagBits::e32; // 32x multisampling
		LOG_DEBUG("MSAAOptions::X32 / VK_SAMPLE_COUNT_32_BIT");
		break;
	case MSAAOptions::X64:
		m_MSAALevel = vk::SampleCountFlagBits::e64; // 64x multisampling
		LOG_DEBUG("MSAAOptions::X64 / VK_SAMPLE_COUNT_64_BIT");
		break;
	}

	if (m_MSAALevel > m_MSAAMax)
	{
		LOG_WARNING("Requested MSAA is not supported, clamping to max MSAA...");
		m_MSAALevel = m_MSAAMax;
	}

	device.getFeatures(&m_physicalDeviceFeatures);
	if (!m_physicalDeviceFeatures.geometryShader)
	{
		LOG_WARNING("Device does not support geometry shaders, skipping device...");
		return false;
	}

	//m_physicalDeviceFeatures2.pNext = &m_physicalDeviceVK12Features;
	//device.getFeatures2(&m_physicalDeviceFeatures2);

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
	if (device.getSurfaceCapabilitiesKHR(m_windowSurface, &m_windowSurfaceCapabilities) != vk::Result::eSuccess)
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

	vk::DeviceSize minAlignment = m_physicalDeviceProps.limits.minUniformBufferOffsetAlignment;

	LOG_SUCCESS("Device is suitable!");
	return true;
}

void Graphics::InitializeWindow()
{
	LOG_INFO("Initializing GLFW window...");
	glfwSetErrorCallback(GLFWErrorCallback);

	LOG_INFO("Initializing GLFW...");
	if (!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");
	if (!glfwVulkanSupported())
		throw std::runtime_error("GLFW: Vulkan not supported!");

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
	CleanupUpscaleResources();

	for (const auto& buffer : m_manageBuffers)
		m_logicalDevice.destroyBuffer(buffer, nullptr);
	for (const auto& sampler : m_manageSamplers)
		m_logicalDevice.destroySampler(sampler, nullptr);
	for (const auto& imageView : m_manageImageViews)
		m_logicalDevice.destroyImageView(imageView, nullptr);
	for (const auto& image : m_manageImages)
		m_logicalDevice.destroyImage(image, nullptr);
	for (const auto& memory : m_manageMemories)
		m_logicalDevice.freeMemory(memory, nullptr);

	CleanupSceneResources();

	m_logicalDevice.destroyDescriptorPool(m_graphicsDescriptorPool, nullptr);

	m_logicalDevice.destroyDescriptorSetLayout(m_dynamicUBODescriptorSetLayout, nullptr);
	m_logicalDevice.destroyDescriptorSetLayout(m_samplerDescriptorSetLayout, nullptr);

	m_logicalDevice.destroyPipeline(m_pipelineGraphics, nullptr);
	m_logicalDevice.destroyPipelineLayout(m_pipelineLayout, nullptr);
	
	m_logicalDevice.destroyRenderPass(m_renderPass, nullptr);
	
	for (auto i = 0; i < m_mainRenderFinishedSemaphores.size(); i++)
	{
		m_logicalDevice.destroySemaphore(m_mainRenderFinishedSemaphores[i], nullptr);
		m_logicalDevice.destroySemaphore(m_imguiRenderFinishedSemaphores[i], nullptr);
	}

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

VKAPI_ATTR vk::Bool32 VKAPI_CALL Graphics::VulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
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

void Graphics::GLFWErrorCallback(int error, const char* description)
{
	LOG_FATAL("GLFW Error!");
	LOG_FATAL(error);
	LOG_FATAL(description);
}
