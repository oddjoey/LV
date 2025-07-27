#include "Graphics.h"

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
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffers();
	CreateSyncObjects();

	LOG_SUCCESS("Graphics initialized successfully!");
}

void Graphics::CreateVulkanInstance()
{
	LOG_INFO("Creating Vulkan instance...");

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Lordi";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Lordi Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = 0;
	instanceCreateInfo.pNext = nullptr;

	uint32_t extensionCount = 0;
	const char** ppExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
	if (!extensionCount)
		throw std::runtime_error("Required GLFW extensions not found!");

	std::unique_ptr<std::vector<const char*>> requiredExtensions = std::make_unique<std::vector<const char*>>(ppExtensions, ppExtensions + extensionCount);

#ifdef _DEBUG
	requiredExtensions->push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(cRequiredValidationLayers.size());
	instanceCreateInfo.ppEnabledLayerNames = cRequiredValidationLayers.data();

	m_DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	m_DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	m_DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	m_DebugCreateInfo.pfnUserCallback = DebugCallback;

	instanceCreateInfo.pNext = &m_DebugCreateInfo;
#endif // _DEBUG
	
	LOG_DEBUG(*requiredExtensions.get());

	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions->size());
	instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions->data();

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_pVulkanInstance) != VK_SUCCESS)
		throw std::runtime_error("Failed to create vulkan instance");

	LOG_SUCCESS("Vulkan instance created successfully!");
}

void Graphics::GetVulkanData()
{
	LOG_INFO("Getting Vulkan data...");

	uint32_t instanceExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
	if (!instanceExtensionCount)
		throw std::runtime_error("No Vulkan extensions found");

	auto extensions = std::make_unique<std::vector<VkExtensionProperties>>(instanceExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensions->data());
	m_pInstanceExtensions.swap(extensions);

	auto instanceExtensionNames = std::make_unique<std::vector<std::string>>(instanceExtensionCount);
	int i = 0;
	for (const auto& extension : *m_pInstanceExtensions)
		instanceExtensionNames->data()[i++] = std::string(extension.extensionName);
	LOG_DEBUG(*instanceExtensionNames.get());

	uint32_t instanceLayerCount = 0;
	vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
	if (!instanceLayerCount)
		throw std::runtime_error("No Vulkan layers found");

	auto layers = std::make_unique<std::vector<VkLayerProperties>>(instanceLayerCount);
	vkEnumerateInstanceLayerProperties(&instanceLayerCount, layers->data());
	m_pInstanceLayers.swap(layers);

	auto instanceLayerNames = std::make_unique<std::vector<std::string>>(instanceLayerCount);
	i = 0;
	for (const auto& layer : *m_pInstanceLayers)
		instanceLayerNames->data()[i++] = std::string(layer.layerName);
	LOG_DEBUG(*instanceLayerNames.get());

	LOG_SUCCESS("Vulkan data retrieved successfully!");
}

void Graphics::PickPhysicalDevice()
{
	LOG_INFO("Picking physical device (GPU)...");

	unsigned int deviceCount = 0;
	vkEnumeratePhysicalDevices(m_pVulkanInstance, &deviceCount, nullptr);
	if (deviceCount == 0)
		throw std::runtime_error("No device found with Vulkan support");

	LOG_DEBUG(deviceCount);

	auto m_Devices = std::make_unique<std::vector<VkPhysicalDevice>>(deviceCount);
	vkEnumeratePhysicalDevices(m_pVulkanInstance, &deviceCount, m_Devices->data());

	for (const auto& device : *m_Devices)
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
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { m_uiDeviceGraphicsIndex, m_uiDevicePresentIndex, m_uiDeviceTransferIndex };

	float queuePriority = 1.0f; // higher values indicate higher priority
	for (auto queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(cRequiredDeviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = cRequiredDeviceExtensions.data();

	deviceCreateInfo.enabledLayerCount = 0;
#ifdef _DEBUG
	deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(cRequiredValidationLayers.size());
	deviceCreateInfo.ppEnabledLayerNames = cRequiredValidationLayers.data();
#endif // _DEBUG

	if (vkCreateDevice(m_pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pLogicalDevice) != VK_SUCCESS)
		throw std::runtime_error("failed to create logical device!");

	vkGetDeviceQueue(m_pLogicalDevice, m_uiDeviceGraphicsIndex, 0, &m_pGraphicsQueue);
	vkGetDeviceQueue(m_pLogicalDevice, m_uiDevicePresentIndex, 0, &m_pPresentQueue);
	vkGetDeviceQueue(m_pLogicalDevice, m_uiDeviceTransferIndex, 0, &m_pTransferQueue);

	LOG_SUCCESS("Logical device created successfully!");
}

void Graphics::CreateSwapChain()
{
	LOG_INFO("Creating swap chain...");

	LOG_DEBUG("Setting swapchain's format (VK_FORMAT_B8G8R8A8_SRGB) and color space (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)");
	// format of the swap chain images, color channels and types
	VkSurfaceFormatKHR swapChainFormat = m_pSwapChainFormats->data()[0];
	// surface format specifications (color format and color space)
	bool formatFound = false;
	for (const auto& format : *m_pSwapChainFormats)
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			LOG_DEBUG("Found required format for swapchain!");
			formatFound = true;
			swapChainFormat = format;
			break;
		}
	if (!formatFound)
		LOG_WARNING("Requested swapchain format/color space not found on device, using device's first format/color format...");

	LOG_DEBUG("Setting swapchain's present mode to user requested present mode:");
	// get user setting and map to a present mode
	auto settings = Settings::Get()->GetGraphicsSettings();
	VkPresentModeKHR userPresentMode = VK_PRESENT_MODE_FIFO_KHR; // default to FIFO (VSync)
	switch (settings.eVSync)
	{
		case VSyncOptions::NONE:
			userPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // No VSync
			LOG_DEBUG("VSyncOptions::NONE / VK_PRESENT_MODE_IMMEDIATE_KHR");
			break;
		case VSyncOptions::ENABLED:
			userPresentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync
			LOG_DEBUG("VSyncOptions::ENABLED / VK_PRESENT_MODE_FIFO_KHR");
			break;
		case VSyncOptions::RELAXED:
			userPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR; // Relaxed VSync
			LOG_DEBUG("VSyncOptions::RELAXED / VK_PRESENT_MODE_FIFO_RELAXED_KHR");
			break;
		case VSyncOptions::MAILBOX:
			userPresentMode = VK_PRESENT_MODE_MAILBOX_KHR; // Mailbox VSync
			LOG_DEBUG("VSyncOptions::MAILBOX / VK_PRESENT_MODE_MAILBOX_KHR");
			break;
	}

	// surface present mode specifications (how images are presented to the window) (VSync)
	VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	bool presentFound = false;
	for (const auto& presentMode : *m_pSwapChainPresentModes)
		if (presentMode == userPresentMode)
		{
			LOG_DEBUG("Found requested swapchain present mode!");
			presentFound = true;
			swapChainPresentMode = presentMode;
			break;
		}
	if (!presentFound)
		LOG_WARNING("Requested present mode not found on device, using defaults (FIFO / VYSNC) as its guaranteed...");

	LOG_DEBUG("Setting swapchain's extent (resolution of images):");
	// resolution of the swap chain images, width and height
	VkExtent2D swapChainExtent = {};
	// surface swap extent specifications (resolution of the swap chain images)
	if (m_SwapChainCapabilities.currentExtent.width == UINT32_MAX)
	{
		// get window size
		int width, height;
		glfwGetFramebufferSize(m_pWindow, &width, &height);
		swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), m_SwapChainCapabilities.minImageExtent.width, m_SwapChainCapabilities.maxImageExtent.width);
		swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), m_SwapChainCapabilities.minImageExtent.height, m_SwapChainCapabilities.maxImageExtent.height);
	}
	else
		// Use the current extent
		swapChainExtent = m_SwapChainCapabilities.currentExtent;

	m_SwapChainCreateInfo.imageExtent = swapChainExtent;
	
	std::pair<uint32_t, uint32_t> outputSwapChainExtent = { swapChainExtent.width, swapChainExtent.height };
	LOG_DEBUG(outputSwapChainExtent);

	uint32_t swapChainImageCount = m_SwapChainCapabilities.minImageCount + 1;
	if (m_SwapChainCapabilities.maxImageCount > 0 && swapChainImageCount > m_SwapChainCapabilities.maxImageCount)
	{
		LOG_WARNING("Swap chain image count exceeds maximum, clamping to maximum...");
		swapChainImageCount = m_SwapChainCapabilities.maxImageCount;
	}
	LOG_DEBUG(swapChainImageCount);

	// Fill swap chain creation info structure
	m_SwapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	m_SwapChainCreateInfo.surface = m_pWindowSurface;
	m_SwapChainCreateInfo.minImageCount = swapChainImageCount;
	m_SwapChainCreateInfo.imageFormat = swapChainFormat.format;
	m_SwapChainCreateInfo.imageColorSpace = swapChainFormat.colorSpace;
	m_SwapChainCreateInfo.imageExtent = swapChainExtent;
	// number of layers in the swap chain images, 1 for single-layer images, more for stereoscopic 3D or VR
	m_SwapChainCreateInfo.imageArrayLayers = 1;
	// usage of the swap chain images, color attachment for rendering
	// to render image to another image, we need to use VK_IMAGE_USAGE_TRANSFER_DST_BIT
	// TO-DO: Research further about swap chain image usage types for post processing effects
	m_SwapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	LOG_DEBUG(m_SwapChainCreateInfo.surface);
	LOG_DEBUG(m_SwapChainCreateInfo.minImageCount);
	LOG_DEBUG(m_SwapChainCreateInfo.imageFormat);
	LOG_DEBUG(m_SwapChainCreateInfo.imageColorSpace);
	LOG_DEBUG(outputSwapChainExtent);
	LOG_DEBUG(m_SwapChainCreateInfo.imageArrayLayers);
	LOG_DEBUG(m_SwapChainCreateInfo.imageUsage);

	if (m_uiDeviceGraphicsIndex != m_uiDevicePresentIndex || m_uiDeviceGraphicsIndex != m_uiDeviceTransferIndex)
	{
		LOG_WARNING("Needed features use seperate queue families, using concurrent mode for swap chain images.");
		// If the graphics queue family is different from the present queue family, we need to specify the queue families
		std::vector<uint32_t> queueFamilyIndices = { m_uiDeviceGraphicsIndex, m_uiDevicePresentIndex, m_uiDeviceTransferIndex };
		m_SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // images can be shared between multiple queues
		m_SwapChainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()); // number of queue families
		m_SwapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data(); // pointer to the queue family indices
	}
	else
	{
		LOG_INFO("Required features use the same queue family, using exclusive mode for swap chain images.");
		// If the graphics queue family is the same as the present queue family, we can use exclusive mode
		m_SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // images are only used by one queue at a time
		m_SwapChainCreateInfo.queueFamilyIndexCount = 0; // no queue families
		m_SwapChainCreateInfo.pQueueFamilyIndices = nullptr; // no queue family indices
	}

	LOG_DEBUG(m_SwapChainCreateInfo.imageSharingMode);
	LOG_DEBUG(m_SwapChainCreateInfo.queueFamilyIndexCount);
	LOG_DEBUG(m_SwapChainCreateInfo.pQueueFamilyIndices);

	// no transformation
	m_SwapChainCreateInfo.preTransform = m_SwapChainCapabilities.currentTransform;
	// ignore alpha blending
	m_SwapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// presentation mode (VSync)
	m_SwapChainCreateInfo.presentMode = swapChainPresentMode;
	// clipped swap chain images provide the best performance compared to non-clipped images
	// clipped images are not presented if they are obscured by other windows
	m_SwapChainCreateInfo.clipped = VK_TRUE;
	// if recreating swapchain, provide previous one here
	// TO-DO: Implement swap chain recreation logic
	m_SwapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	LOG_DEBUG(m_SwapChainCreateInfo.preTransform);
	LOG_DEBUG(m_SwapChainCreateInfo.compositeAlpha);
	LOG_DEBUG(m_SwapChainCreateInfo.presentMode);
	LOG_DEBUG(m_SwapChainCreateInfo.clipped);
	LOG_DEBUG(m_SwapChainCreateInfo.oldSwapchain);

	if (vkCreateSwapchainKHR(m_pLogicalDevice, &m_SwapChainCreateInfo, nullptr, &m_pSwapChain) != VK_SUCCESS)
		throw std::runtime_error("failed to create swapchain!");

	LOG_DEBUG("Getting swapchain image count:");
	// Get swap chain images
	vkGetSwapchainImagesKHR(m_pLogicalDevice, m_pSwapChain, &swapChainImageCount, nullptr);
	if (!swapChainImageCount)
		throw std::runtime_error("No swapchain images found!");
	LOG_DEBUG(swapChainImageCount);

	auto swapChainImages = std::make_unique<std::vector<VkImage>>(swapChainImageCount);
	vkGetSwapchainImagesKHR(m_pLogicalDevice, m_pSwapChain, &swapChainImageCount, swapChainImages->data());
	m_pSwapChainImages.swap(swapChainImages);

	LOG_SUCCESS("Swapchain created successfully!");
}

void Graphics::CreateImageViews()
{
	LOG_INFO("Creating image views for swapchain images...");

	m_pSwapChainImageViews = std::make_unique<std::vector<VkImageView>>(m_pSwapChainImages->size());

	LOG_DEBUG("Number of image views to create (swapchain images):");
	LOG_DEBUG(m_pSwapChainImages->size());
	for (auto i = 0; i < m_pSwapChainImages->size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = m_pSwapChainImages->data()[i];
		// 1D texture, 2D texture, 3D texture, cube map, etc.
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = m_SwapChainCreateInfo.imageFormat;
		// components allows manipulation of the color channels of the image
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		// subresource range specifies which part of the image to use aswell as mipmapping and array layers
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // color aspect
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0; // base mip level
		imageViewCreateInfo.subresourceRange.levelCount = 1; // number of mip levels to use
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0; // base array layer
		imageViewCreateInfo.subresourceRange.layerCount = 1; // number of array layers to use

		if (vkCreateImageView(m_pLogicalDevice, &imageViewCreateInfo, nullptr, &m_pSwapChainImageViews->data()[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create image view for swapchain image!");

		LOG_DEBUG("Created image view for swapchain image:");
		LOG_DEBUG(i);
		LOG_DEBUG(imageViewCreateInfo.image);
		LOG_DEBUG(imageViewCreateInfo.viewType);
		LOG_DEBUG(imageViewCreateInfo.format);
		LOG_DEBUG(imageViewCreateInfo.components.r);
		LOG_DEBUG(imageViewCreateInfo.components.g);
		LOG_DEBUG(imageViewCreateInfo.components.b);
		LOG_DEBUG(imageViewCreateInfo.components.a);
		LOG_DEBUG(imageViewCreateInfo.subresourceRange.aspectMask);
		LOG_DEBUG(imageViewCreateInfo.subresourceRange.baseMipLevel);
		LOG_DEBUG(imageViewCreateInfo.subresourceRange.levelCount);
		LOG_DEBUG(imageViewCreateInfo.subresourceRange.baseArrayLayer);
		LOG_DEBUG(imageViewCreateInfo.subresourceRange.layerCount);
	}

	LOG_SUCCESS("Image views for swapchain images created successfully!");
}

void Graphics::CreateRenderPass()
{
	LOG_INFO("Creating render pass...");

	LOG_DEBUG("Creating render pass attachment description:");
	// Render pass is a collection of attachments (metadata about images) and subpasses (rendering operations)
	// Attachments defines how images are used during rendering
	// each attachment adds data to each pixel, depending on attachment type
	VkAttachmentDescription colorAttachmentDesc = {};
	colorAttachmentDesc.format = m_SwapChainCreateInfo.imageFormat; // format of the color attachment
	LOG_DEBUG(colorAttachmentDesc.format);

	LOG_DEBUG("Setting multisampling for color attachment based on user's settings:");
	auto settings = Settings::Get()->GetGraphicsSettings();
	switch (settings.eMSAA)
	{
		case MSAAOptions::X1:
			m_eUserMSAA = VK_SAMPLE_COUNT_1_BIT; // no multisampling
			LOG_DEBUG("MSAAOptions::X1 / VK_SAMPLE_COUNT_1_BIT");
			break;
		case MSAAOptions::X2:
			m_eUserMSAA = VK_SAMPLE_COUNT_2_BIT; // 2x multisampling
			LOG_DEBUG("MSAAOptions::X2 / VK_SAMPLE_COUNT_2_BIT");
			break;
		case MSAAOptions::X4:
			m_eUserMSAA = VK_SAMPLE_COUNT_4_BIT; // 4x multisampling
			LOG_DEBUG("MSAAOptions::X4 / VK_SAMPLE_COUNT_4_BIT");
			break;
		case MSAAOptions::X8:
			m_eUserMSAA = VK_SAMPLE_COUNT_8_BIT; // 8x multisampling
			LOG_DEBUG("MSAAOptions::X8 / VK_SAMPLE_COUNT_8_BIT");
			break;
		case MSAAOptions::X16:
			m_eUserMSAA = VK_SAMPLE_COUNT_16_BIT; // 16x multisampling
			LOG_DEBUG("MSAAOptions::X16 / VK_SAMPLE_COUNT_16_BIT");
			break;
		case MSAAOptions::X32:
			m_eUserMSAA = VK_SAMPLE_COUNT_32_BIT; // 32x multisampling
			LOG_DEBUG("MSAAOptions::X32 / VK_SAMPLE_COUNT_32_BIT");
			break;
		case MSAAOptions::X64:
			m_eUserMSAA = VK_SAMPLE_COUNT_64_BIT; // 64x multisampling
			LOG_DEBUG("MSAAOptions::X64 / VK_SAMPLE_COUNT_64_BIT");
			break;
	}

	colorAttachmentDesc.samples = m_eUserMSAA; // number of samples per pixel (multisampling)

	// value specifying how the contents of color and depth components of the attachment are treated at the beginning of the subpass where it is first used.
	// VK_ATTACHMENT_LOAD_OP_LOAD (preserves data)
	// VK_ATTACHMENT_LOAD_OP_CLEAR (clears data)
	// VK_ATTACHMENT_LOAD_OP_DONT_CARE (ignores the data)
	colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	LOG_DEBUG(colorAttachmentDesc.loadOp);

	// value specifying how the contents of color and depth components of the attachment are treated at the end of the subpass where it is last used.
	// VK_ATTACHMENT_STORE_OP_STORE stores the data in the attachment in memory to be used later
	// VK_ATTACHMENT_STORE_OP_DONT_CARE ignores the data in the attachment, data is not preserved
	colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store the attachment after rendering
	LOG_DEBUG(colorAttachmentDesc.storeOp);

	colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // no stencil buffer
	colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // no stencil buffer
	LOG_DEBUG(colorAttachmentDesc.stencilLoadOp);
	LOG_DEBUG(colorAttachmentDesc.stencilStoreOp);

	// VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
	//  -   images used as color attachments during rendering
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	//  -   images used for presentation to the screen aka for the swap chain)
	// VK_IMAGE_LAYOUT_UNDEFINED 
	//  -   dont care about initial layout, we'll set it in finalLayout, also data not guaranteed to be preserved
	// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
	//  -   images used for transfer operations (e.g. blitting, copying, etc.)
	colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // layout the attachment image subresource will be in when a render pass instance begins.
	colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // layout the attachment image subresource will be transitioned to when a render pass instance ends.
	LOG_DEBUG(colorAttachmentDesc.initialLayout);
	LOG_DEBUG(colorAttachmentDesc.finalLayout);

	// Attachment reference is used to specify how the attachment is used in the subpass
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0; // integer value identifying an attachment at the corresponding index
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // value specifying the layout the attachment uses during the subpass
	LOG_DEBUG(colorAttachmentRef.attachment);
	LOG_DEBUG(colorAttachmentRef.layout);

	// A single render pass can have multiple subpasses,
	// subpasses are subsequent rendering operations that depend on the contents
	// of the previous framebuffers from previous passes. e.g. post-processing effects
	// other types of attachments:
	// pInputAttachments - input attachments (used as input to the subpass)
	// pResolveAttachments - resolve attachments (used for multisampling)
	// pDepthStencilAttachment - depth/stencil attachments (used for depth/stencil testing)
	// pPreserveAttachments - preserve attachments (used to preserve the contents of the attachment)
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // subpass is a graphics pipeline
	LOG_DEBUG(subpass.pipelineBindPoint);

	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	LOG_DEBUG(subpass.inputAttachmentCount);
	LOG_DEBUG(subpass.pInputAttachments);

	subpass.colorAttachmentCount = 1; 
	subpass.pColorAttachments = &colorAttachmentRef;
	LOG_DEBUG(subpass.colorAttachmentCount);
	LOG_DEBUG(subpass.pColorAttachments);

	subpass.pResolveAttachments = nullptr;
	LOG_DEBUG(subpass.pResolveAttachments);

	subpass.pDepthStencilAttachment = nullptr;
	LOG_DEBUG(subpass.pDepthStencilAttachment);

	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;
	LOG_DEBUG(subpass.preserveAttachmentCount);
	LOG_DEBUG(subpass.pPreserveAttachments);

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // subpass index of the first subpass in the dependency, or VK_SUBPASS_EXTERNAL
	dependency.dstSubpass = 0; // subpass index of the second subpass in the dependency, or VK_SUBPASS_EXTERNAL.
	LOG_DEBUG(dependency.srcSubpass);
	LOG_DEBUG(dependency.dstSubpass);

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 
	dependency.srcAccessMask = 0; // 
	LOG_DEBUG(dependency.srcStageMask);
	LOG_DEBUG(dependency.srcAccessMask);

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // destination stage is color attachment output
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // destination access mask is color attachment write
	LOG_DEBUG(dependency.dstStageMask);
	LOG_DEBUG(dependency.dstAccessMask);

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1; // number of attachments used by this render pass.
	renderPassInfo.pAttachments = &colorAttachmentDesc; // pointer to the attachments
	renderPassInfo.subpassCount = 1; // number of subpasses
	renderPassInfo.pSubpasses = &subpass; // pointer
	renderPassInfo.dependencyCount = 1; // number of memory dependencies between pairs of subpasses.
	renderPassInfo.pDependencies = &dependency; // pointer to the dependencies
	LOG_DEBUG(renderPassInfo.attachmentCount);
	LOG_DEBUG(renderPassInfo.pAttachments);
	LOG_DEBUG(renderPassInfo.subpassCount);
	LOG_DEBUG(renderPassInfo.pSubpasses);
	LOG_DEBUG(renderPassInfo.dependencyCount);
	LOG_DEBUG(renderPassInfo.pDependencies);

	if (vkCreateRenderPass(m_pLogicalDevice, &renderPassInfo, nullptr, &m_pRenderPass) != VK_SUCCESS)
		throw std::runtime_error("failed to create render pass!");

	LOG_SUCCESS("Render pass created successfully!");
}

void Graphics::CreateGraphicsPipeline()
{
	LOG_INFO("Creating graphics pipeline...");

	LOG_DEBUG("Creating shader modules for vertex and fragment shaders...");
	// read the compiled shader byte code from files
	auto fragShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/frag.spv");
	auto vertShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/vert.spv");

	// create shader modules from the byte code
	VkShaderModuleCreateInfo fragShaderInfo = {};
	fragShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragShaderInfo.codeSize = fragShaderCode.size();
	fragShaderInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());
	LOG_DEBUG(fragShaderInfo.codeSize);
	LOG_DEBUG(fragShaderInfo.pCode);

	if (vkCreateShaderModule(m_pLogicalDevice, &fragShaderInfo, nullptr, &m_pFragmentShaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create fragment shader module!");

	VkShaderModuleCreateInfo vertShaderInfo = {};
	vertShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderInfo.codeSize = vertShaderCode.size();
	vertShaderInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());
	LOG_DEBUG(vertShaderInfo.codeSize);
	LOG_DEBUG(vertShaderInfo.pCode);

	if (vkCreateShaderModule(m_pLogicalDevice, &vertShaderInfo, nullptr, &m_pVertexShaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create vertex shader module!");

	LOG_DEBUG("Creating vertex shader stage info:");
	// create shader stages for the graphics pipeline
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	// vertex shader stage
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	// shader module
	vertShaderStageInfo.module = m_pVertexShaderModule;
	// entry point of the shader
	vertShaderStageInfo.pName = "main"; 
	// specialization info is used to pass constants to the shader at compile time
	vertShaderStageInfo.pSpecializationInfo = nullptr;
	LOG_DEBUG(vertShaderStageInfo.stage);
	LOG_DEBUG(vertShaderStageInfo.module);
	LOG_DEBUG(vertShaderStageInfo.pName);
	LOG_DEBUG(vertShaderStageInfo.pSpecializationInfo);

	LOG_DEBUG("Creating fragment shader stage info:");
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	// fragment shader stage
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	// shader module
	fragShaderStageInfo.module = m_pFragmentShaderModule;
	// entry point of the shader
	fragShaderStageInfo.pName = "main";
	// specialization info is used to pass constants to the shader at compile time
	fragShaderStageInfo.pSpecializationInfo = nullptr;
	LOG_DEBUG(fragShaderStageInfo.stage);
	LOG_DEBUG(fragShaderStageInfo.module);
	LOG_DEBUG(fragShaderStageInfo.pName);
	LOG_DEBUG(fragShaderStageInfo.pSpecializationInfo);

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
	
	// dynamic states are states that can be changed at runtime
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT, // viewport can be changed at runtime
		VK_DYNAMIC_STATE_SCISSOR // scissor can be changed at runtime
	};

	LOG_DEBUG("Creating dynamic state info:");
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();
	LOG_DEBUG(dynamicStateInfo.dynamicStateCount);
	LOG_DEBUG(dynamicStateInfo.pDynamicStates);

	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptions = Vertex::GetAttributeDescriptions();

	LOG_DEBUG("Creating pipeline vertex input state info:");
	// vertex input state describes the format of the vertex data
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	LOG_DEBUG(vertexInputInfo.vertexBindingDescriptionCount);
	LOG_DEBUG(vertexInputInfo.pVertexBindingDescriptions);
	LOG_DEBUG(vertexInputInfo.vertexAttributeDescriptionCount);
	LOG_DEBUG(vertexInputInfo.pVertexAttributeDescriptions);

	LOG_DEBUG("Creating pipeline input assembly state info:");
	// input assembly state describes how the vertex data is assembled into primitives
	// primitives are the basic building blocks of the graphics pipeline, e.g. points, lines, triangles
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
	inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
	LOG_DEBUG(inputAssemblyInfo.topology);
	LOG_DEBUG(inputAssemblyInfo.primitiveRestartEnable);

	LOG_DEBUG("Creating pipeline viewport state info:");
	// viewport state describes the viewport and scissor rectangles, 
	// since dynamic states are used, we only need to specify the count
	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1; // number of viewports
	viewportStateInfo.scissorCount = 1; // number of scissors
	LOG_DEBUG(viewportStateInfo.viewportCount);
	LOG_DEBUG(viewportStateInfo.scissorCount);

	LOG_DEBUG("Creating pipeline rasterization state info:");
	// rasterization state describes how the primitives are rasterized (converted to pixels)
	VkPipelineRasterizationStateCreateInfo rasterizerInfo = {};
	rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	// should fragments that are beyond the near and far planes be clamped
	// TO-DO: If true, a GPU feature needs to be enabled
	rasterizerInfo.depthClampEnable = VK_FALSE;
	// disable output to framebuffer (geometry never passes through the rasterizer)
	rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
	// specifies how the primitives are drawn, e.g.
	// VK_POLYGON_MODE_FILL for filled triangles,
	// VK_POLYGON_MODE_LINE for wireframe triangles, and
	// VK_POLYGON_MODE_POINT for points
	// TO-DO: Anything other than VK_POLYGON_MODE_FILL requires a GPU feature to be enabled
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
	// line width in fragments
	// TO-DO: if thicker than 1.0f, a GPU feature (wideLines) needs to be enabled
	rasterizerInfo.lineWidth = 1.0f; // line width
	// specifies how the triangles are culled, e.g.
	// VK_CULL_MODE_NONE for no culling,
	// VK_CULL_MODE_FRONT_BIT for front face culling,
	// VK_CULL_MODE_BACK_BIT for back face culling, and
	// VK_CULL_MODE_FRONT_AND_BACK for both front and back face culling
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	// specifies the front face of the triangles, e.g.
	// VK_FRONT_FACE_COUNTER_CLOCKWISE for counter-clockwise front face,
	// VK_FRONT_FACE_CLOCKWISE for clockwise front face
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	// specifies if depth bias is enabled, used for shadow mapping to avoid shadow acne
	rasterizerInfo.depthBiasEnable = VK_FALSE; // controls whether to bias fragment depth values
	rasterizerInfo.depthBiasConstantFactor = 0.0f; // scalar factor controlling the constant depth value added to each fragment
	rasterizerInfo.depthBiasClamp = 0.0f; // maximum (or minimum) depth bias of a fragment
	rasterizerInfo.depthBiasSlopeFactor = 0.0f; // scalar factor applied to a fragment’s slope in depth bias calculations
	LOG_DEBUG(rasterizerInfo.depthClampEnable);
	LOG_DEBUG(rasterizerInfo.rasterizerDiscardEnable);
	LOG_DEBUG(rasterizerInfo.polygonMode);
	LOG_DEBUG(rasterizerInfo.lineWidth);
	LOG_DEBUG(rasterizerInfo.cullMode);
	LOG_DEBUG(rasterizerInfo.frontFace);
	LOG_DEBUG(rasterizerInfo.depthBiasEnable);
	LOG_DEBUG(rasterizerInfo.depthBiasConstantFactor);
	LOG_DEBUG(rasterizerInfo.depthBiasClamp);
	LOG_DEBUG(rasterizerInfo.depthBiasSlopeFactor);

	LOG_DEBUG("Creating pipeline multisampling state info:");
	// multisampling state describes how the fragments are sampled, e.g. anti-aliasing
	// multisampling is used to reduce aliasing artifacts in the rendered image
	// works by sampling multiple fragments per pixel and averaging the results
	// TO-DO: If multisampling is enabled, GPU feature (sampleRateShading) needs to be enabled
	VkPipelineMultisampleStateCreateInfo multisamplingInfo = {};
	multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingInfo.sampleShadingEnable = VK_FALSE;
	multisamplingInfo.rasterizationSamples = m_eUserMSAA;
	multisamplingInfo.minSampleShading = 1.0f;
	multisamplingInfo.pSampleMask = nullptr;
	multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingInfo.alphaToOneEnable = VK_FALSE;
	LOG_DEBUG(multisamplingInfo.sampleShadingEnable);
	LOG_DEBUG(multisamplingInfo.rasterizationSamples);
	LOG_DEBUG(multisamplingInfo.minSampleShading);
	LOG_DEBUG(multisamplingInfo.pSampleMask);
	LOG_DEBUG(multisamplingInfo.alphaToCoverageEnable);
	LOG_DEBUG(multisamplingInfo.alphaToOneEnable);

	// depth stencil state describes how the depth and stencil buffers are used
	// VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};

	auto settings = Settings::Get()->GetGraphicsSettings();

	LOG_DEBUG("Creating pipeline color blend attachment state info:");
	// color blending state describes how the colors of the fragments are blended with the colors of the framebuffer.
	// attachment state describes how the color attachment is blended for each framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = settings.eColorBlending == ColorBlendingOptions::MIX ? VK_TRUE : VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	LOG_DEBUG(colorBlendAttachment.colorWriteMask);
	LOG_DEBUG(colorBlendAttachment.blendEnable);
	LOG_DEBUG(colorBlendAttachment.srcColorBlendFactor);
	LOG_DEBUG(colorBlendAttachment.dstColorBlendFactor);
	LOG_DEBUG(colorBlendAttachment.colorBlendOp);
	LOG_DEBUG(colorBlendAttachment.srcAlphaBlendFactor);
	LOG_DEBUG(colorBlendAttachment.dstAlphaBlendFactor);
	LOG_DEBUG(colorBlendAttachment.alphaBlendOp);

	LOG_DEBUG("Creating pipeline color blend state info:");
	// global color blending settings
	VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {};
	colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateInfo.logicOpEnable = settings.eColorBlending == ColorBlendingOptions::BITWISE ? VK_TRUE : VK_FALSE;
	colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateInfo.attachmentCount = 1;
	colorBlendStateInfo.pAttachments = &colorBlendAttachment;
	colorBlendStateInfo.blendConstants[0] = 0.0f;
	colorBlendStateInfo.blendConstants[1] = 0.0f;
	colorBlendStateInfo.blendConstants[2] = 0.0f;
	colorBlendStateInfo.blendConstants[3] = 0.0f;
	LOG_DEBUG(colorBlendStateInfo.logicOpEnable);
	LOG_DEBUG(colorBlendStateInfo.logicOp);
	LOG_DEBUG(colorBlendStateInfo.attachmentCount);
	LOG_DEBUG(colorBlendStateInfo.pAttachments);
	LOG_DEBUG(colorBlendStateInfo.blendConstants[0]);
	LOG_DEBUG(colorBlendStateInfo.blendConstants[1]);
	LOG_DEBUG(colorBlendStateInfo.blendConstants[2]);
	LOG_DEBUG(colorBlendStateInfo.blendConstants[3]);

	LOG_DEBUG("Creating pipeline layout info:");
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_pDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0; // no push constants
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // no push constants
	LOG_DEBUG(pipelineLayoutInfo.setLayoutCount);
	LOG_DEBUG(pipelineLayoutInfo.pSetLayouts);
	LOG_DEBUG(pipelineLayoutInfo.pushConstantRangeCount);
	LOG_DEBUG(pipelineLayoutInfo.pPushConstantRanges);

	LOG_DEBUG("Creating pipeline layout:");
	if (vkCreatePipelineLayout(m_pLogicalDevice, &pipelineLayoutInfo, nullptr, &m_pPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");
	LOG_SUCCESS("Pipeline layout created successfully!");

	LOG_DEBUG("Creating graphics pipeline info:");
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizerInfo;
	pipelineInfo.pMultisampleState = &multisamplingInfo;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlendStateInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = m_pPipelineLayout;
	pipelineInfo.renderPass = m_pRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;
	LOG_DEBUG(pipelineInfo.stageCount);
	LOG_DEBUG(pipelineInfo.pStages);
	LOG_DEBUG(pipelineInfo.pVertexInputState);
	LOG_DEBUG(pipelineInfo.pInputAssemblyState);
	LOG_DEBUG(pipelineInfo.pViewportState);
	LOG_DEBUG(pipelineInfo.pRasterizationState);
	LOG_DEBUG(pipelineInfo.pMultisampleState);
	LOG_DEBUG(pipelineInfo.pDepthStencilState);
	LOG_DEBUG(pipelineInfo.pColorBlendState);
	LOG_DEBUG(pipelineInfo.pDynamicState);
	LOG_DEBUG(pipelineInfo.layout);
	LOG_DEBUG(pipelineInfo.renderPass);
	LOG_DEBUG(pipelineInfo.subpass);
	LOG_DEBUG(pipelineInfo.basePipelineHandle);
	LOG_DEBUG(pipelineInfo.basePipelineIndex);

	if (vkCreateGraphicsPipelines(m_pLogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pGraphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	// cleanup shader modules
	vkDestroyShaderModule(m_pLogicalDevice, m_pFragmentShaderModule, nullptr);
	vkDestroyShaderModule(m_pLogicalDevice, m_pVertexShaderModule, nullptr);

	LOG_SUCCESS("Graphics pipeline created successfully!");
}

void Graphics::CreateFrameBuffers()
{
	LOG_INFO("Creating frame buffers for swapchain image views...");

	auto frameBuffers = std::make_unique<std::vector<VkFramebuffer>>(m_pSwapChainImageViews->size());

	LOG_DEBUG("Number of frame buffers to create (swapchain image views):");
	LOG_DEBUG(m_pSwapChainImageViews->size());
	for (auto i = 0; i < m_pSwapChainImageViews->size(); i++)
	{
		VkImageView attachments[] = {
				   m_pSwapChainImageViews->data()[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_pRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_SwapChainCreateInfo.imageExtent.width;
		framebufferInfo.height = m_SwapChainCreateInfo.imageExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_pLogicalDevice, &framebufferInfo, nullptr, &frameBuffers->data()[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create framebuffer!");

		LOG_DEBUG("Created framebuffer for swapchain image view:");
		LOG_DEBUG(i);
		LOG_DEBUG(framebufferInfo.renderPass);
		LOG_DEBUG(framebufferInfo.attachmentCount);
		LOG_DEBUG(framebufferInfo.pAttachments);
		LOG_DEBUG(framebufferInfo.width);
		LOG_DEBUG(framebufferInfo.height);
		LOG_DEBUG(framebufferInfo.layers);
	}

	m_pFrameBuffers.swap(frameBuffers);
	LOG_SUCCESS("Frame buffers created successfully!");
}

void Graphics::CreateCommandPools()
{
	LOG_INFO("Creating command pools...");

	LOG_DEBUG("Creating graphics pool...");
	VkCommandPoolCreateInfo drawingPoolInfo = {};
	drawingPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = command buffers are rerecorded often
	// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = command buffers individually rerecorded
	drawingPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	drawingPoolInfo.queueFamilyIndex = m_uiDeviceGraphicsIndex;
	LOG_DEBUG(drawingPoolInfo.flags);
	LOG_DEBUG(drawingPoolInfo.queueFamilyIndex);

	if (vkCreateCommandPool(m_pLogicalDevice, &drawingPoolInfo, nullptr, &m_pDrawingCommandPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create drawing command pool!");
	LOG_DEBUG("Sucessfully created graphics pool!");

	LOG_DEBUG("Creating transfer pool...");
	VkCommandPoolCreateInfo transferPoolInfo = {};
	transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	transferPoolInfo.queueFamilyIndex = m_uiDeviceTransferIndex;
	LOG_DEBUG(transferPoolInfo.flags);
	LOG_DEBUG(transferPoolInfo.queueFamilyIndex);

	if (vkCreateCommandPool(m_pLogicalDevice, &transferPoolInfo, nullptr, &m_pTransferCommandPool) != VK_SUCCESS)
		throw std::runtime_error("failed to create transfer command pool!");
	LOG_DEBUG("Sucessfully created transfer pool!");

	LOG_SUCCESS("Successfully created command pools!");
}

void Graphics::CreateCommandBuffers()
{
	LOG_INFO("Creating command buffer...");

	auto commandBuffers = std::make_unique<std::vector<VkCommandBuffer>>(cMaxFramesInFlight);

	LOG_DEBUG("Allocating command buffer from command pool:");
	VkCommandBufferAllocateInfo allocateInfo = {}; // allocation parameters for the command buffer
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_pDrawingCommandPool; // command pool from which the command buffers are allocated
	// VK_COMMAND_BUFFER_LEVEL_PRIMARY = can be submitted to queue for exec, but not called from other command buffers.
	// VK_COMMAND_BUFFER_LEVEL_SECONDARY = cannot be submitted to queue but can be called from primary command buffers.
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers->size());
	LOG_DEBUG(allocateInfo.commandPool);
	LOG_DEBUG(allocateInfo.level);
	LOG_DEBUG(allocateInfo.commandBufferCount);

	if (vkAllocateCommandBuffers(m_pLogicalDevice, &allocateInfo, commandBuffers->data()) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate command buffers!");

	m_pCommandBuffers.swap(commandBuffers);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = each recording of the command buffer will only be submitted once,
	// and the command buffer will be reset and recorded again between each submission.
	// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = secondary command buffer is considered to be entirely inside a render pass.
	// If this is a primary command buffer, then this bit is ignored.
	// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = command buffer can be resubmitted to any queue of the same queue family while it is in the pending state,
	// and recorded into multiple primary command buffers.
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // command buffer can be submitted multiple times
	beginInfo.pInheritanceInfo = nullptr;
	LOG_DEBUG(beginInfo.flags);
	LOG_DEBUG(beginInfo.pInheritanceInfo);

	LOG_SUCCESS("Created command buffer successfully!");
}

void Graphics::RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = command buffer is only submitted once right after exec
	// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = secondary command buffer within a render pass
	// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = command buffer can be resubmitted multiple tim es
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr; // no inheritance info from primary command buffers for secondary command buffers

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		throw std::runtime_error("failed to begin recording command buffer!");
	
	// Begin render pass
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_pRenderPass;
	renderPassInfo.framebuffer = m_pFrameBuffers->data()[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 }; // offset of the render area
	renderPassInfo.renderArea.extent = m_SwapChainCreateInfo.imageExtent; // size of the render area

	// clear values for the attachments, e.g. color and depth
	VkClearValue clearColor = {};
	clearColor.color = { { 0.0f, 0.0f, 0.0f, 1.0f } }; 
	renderPassInfo.clearValueCount = 1; 
	renderPassInfo.pClearValues = &clearColor;

	// VK_SUBPASS_CONTENTS_INLINE = commands are recorded inline in the render pass
	// VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS = secondary command buffers are recorded in the render pass
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pGraphicsPipeline);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_SwapChainCreateInfo.imageExtent.width);
	viewport.height = static_cast<float>(m_SwapChainCreateInfo.imageExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = m_SwapChainCreateInfo.imageExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	VkBuffer vertexBuffers[] = { m_pVertexBuffer };
	VkDeviceSize offsets[] = { 0 }; // offset in bytes from the start of the buffer
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // bind vertex buffer to the command buffer

	vkCmdBindIndexBuffer(commandBuffer, m_pIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pPipelineLayout,
		0, 1, &m_pDescriptorSets->data()[m_uiCurrentFrame], 0, nullptr);

	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		throw std::runtime_error("failed to record command buffer!");
}

void Graphics::DrawFrame()
{
	// wait for frame 
	vkWaitForFences(m_pLogicalDevice, 1, &m_pFrameInFlightFences->data()[m_uiCurrentFrame], VK_TRUE, UINT64_MAX);

	// get an image from swap chain
	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(m_pLogicalDevice, m_pSwapChain, UINT64_MAX, m_pImageAvailableSemaphores->data()[m_uiCurrentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	UpdateUniformBuffer(m_uiCurrentFrame);

	vkResetFences(m_pLogicalDevice, 1, &m_pFrameInFlightFences->data()[m_uiCurrentFrame]);

	// record command buffer for the acquired image
	vkResetCommandBuffer(m_pCommandBuffers->data()[m_uiCurrentFrame], 0);
	RecordCommandBuffer(m_pCommandBuffers->data()[m_uiCurrentFrame], imageIndex);

	VkSemaphore waitSemaphores[] = { m_pImageAvailableSemaphores->data()[m_uiCurrentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // wait for color attachment output stage
	VkSemaphore signalSemaphores[] = { m_pRenderFinishedSemaphores->data()[imageIndex] };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_pCommandBuffers->data()[m_uiCurrentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	
	if (vkQueueSubmit(m_pGraphicsQueue, 1, &submitInfo, m_pFrameInFlightFences->data()[m_uiCurrentFrame]) != VK_SUCCESS)
		throw std::runtime_error("failed to submit draw command buffer!");

	VkSwapchainKHR swapChains[] = { m_pSwapChain };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;
	
	result = vkQueuePresentKHR(m_pGraphicsQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_bFramebufferResized)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS)
		throw std::runtime_error("failed to present swap chain image!");

	m_uiCurrentFrame = (m_uiCurrentFrame + 1) % cMaxFramesInFlight; // increment current frame index and wrap around if necessary
}

void Graphics::CreateSyncObjects()
{
	LOG_INFO("Creating synchronization objects...");

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto imageAvailableSemaphore = std::make_unique<std::vector<VkSemaphore>>(cMaxFramesInFlight, VK_NULL_HANDLE);
	auto inFlightFences = std::make_unique<std::vector<VkFence>>(cMaxFramesInFlight, VK_NULL_HANDLE);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // fence is signaled when created
	LOG_DEBUG(fenceInfo.flags);

	LOG_DEBUG("Creating image available semaphores and in flight fences:");
	LOG_DEBUG(cMaxFramesInFlight);
	for (auto i = 0; i < cMaxFramesInFlight; i++)
	{
		if (vkCreateSemaphore(m_pLogicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore->data()[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create image available semaphore!");
		if (vkCreateFence(m_pLogicalDevice, &fenceInfo, nullptr, &inFlightFences->data()[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create in flight fence!");

		LOG_DEBUG("Created semaphore and fence for in flight frame:");
		LOG_DEBUG(i);
		LOG_DEBUG(imageAvailableSemaphore->data()[i]);
		LOG_DEBUG(inFlightFences->data()[i]);
	}

	m_pImageAvailableSemaphores.swap(imageAvailableSemaphore);
	m_pFrameInFlightFences.swap(inFlightFences);

	auto renderFinishedSemaphore = std::make_unique<std::vector<VkSemaphore>>(m_pSwapChainImages->size(), VK_NULL_HANDLE);

	LOG_DEBUG("Creating render finished semaphore for each swap chain image:");
	LOG_DEBUG(m_pSwapChainImages->size());
	for (auto i = 0; i < m_pSwapChainImages->size(); i++)
	{
		if (vkCreateSemaphore(m_pLogicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore->data()[i]) != VK_SUCCESS)
			throw std::runtime_error("failed to create render finished semaphore for swap chain image!");
		LOG_DEBUG("Created render finished semaphore for swap chain image:");
		LOG_DEBUG(i);
		LOG_DEBUG(renderFinishedSemaphore->data()[i]);
	}
	m_pRenderFinishedSemaphores.swap(renderFinishedSemaphore);

	LOG_SUCCESS("Synchronization objects created successfully!");
}

void Graphics::RecreateSwapChain()
{
	LOG_INFO("Recreating swapchain...");

	// check if window is minimized	
	int width = 0, height = 0;
	glfwGetFramebufferSize(m_pWindow, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(m_pWindow, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(m_pLogicalDevice); // wait for the device to finish all operations

	CleanupSwapChain();

	CreateSwapChain();
	CreateImageViews();
	CreateFrameBuffers();

	LOG_SUCCESS("Swapchain recreated successfully!");
}

void Graphics::CleanupSwapChain()
{
	LOG_INFO("Cleaning up swapchain...");

	for (const auto& framebuffer : *m_pFrameBuffers)
		vkDestroyFramebuffer(m_pLogicalDevice, framebuffer, nullptr);

	for (const auto& imageView : *m_pSwapChainImageViews)
		vkDestroyImageView(m_pLogicalDevice, imageView, nullptr);

	vkDestroySwapchainKHR(m_pLogicalDevice, m_pSwapChain, nullptr);

	LOG_SUCCESS("Swapchain cleaned up successfully!");
}

void Graphics::CreateVertexBuffer()
{
	LOG_INFO("Creating vertex buffer...");

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer = nullptr;
	VkDeviceMemory stagingBufferMemory = nullptr;
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(m_pLogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize)); 
	vkUnmapMemory(m_pLogicalDevice, stagingBufferMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pVertexBuffer, m_pVertexBufferMemory);

	CopyBuffer(stagingBuffer, m_pVertexBuffer, bufferSize);

	vkDestroyBuffer(m_pLogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_pLogicalDevice, stagingBufferMemory, nullptr);

	LOG_SUCCESS("Vertex buffer created successfully!");
}

void Graphics::CreateIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(m_pLogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_pLogicalDevice, stagingBufferMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_pIndexBuffer, m_pIndexBufferMemory);

	CopyBuffer(stagingBuffer, m_pIndexBuffer, bufferSize);

	vkDestroyBuffer(m_pLogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_pLogicalDevice, stagingBufferMemory, nullptr);
}

uint32_t Graphics::FindMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperties)
{
	LOG_INFO("Finding memory type...");

	for (uint32_t i = 0; i < m_PhysicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if (((1 << i) & memoryType) && (m_PhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
		{
			LOG_SUCCESS("Memory type found!");
			return i;
		}
	}

	LOG_FATAL("Failed to find suitable memory type!");
	return 0;
}

void Graphics::CreateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsageFlags,
	VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& deviceMemory)
{
	LOG_INFO("Creating buffer...");

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	LOG_DEBUG(bufferInfo.size);
	LOG_DEBUG(bufferInfo.usage);
	LOG_DEBUG(bufferInfo.sharingMode);

	if (vkCreateBuffer(m_pLogicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		throw std::runtime_error("failed to create vertex buffer!");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_pLogicalDevice, buffer, &memRequirements);
	LOG_DEBUG(memRequirements.size);
	LOG_DEBUG(memRequirements.memoryTypeBits);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryTypeIndex(memRequirements.memoryTypeBits, memoryPropertyFlags);
	LOG_DEBUG(allocInfo.allocationSize);
	LOG_DEBUG(allocInfo.memoryTypeIndex);

	if (vkAllocateMemory(m_pLogicalDevice, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS)
		throw std::runtime_error("failed to allocate vertex buffer memory!");

	vkBindBufferMemory(m_pLogicalDevice, buffer, deviceMemory, 0);

	LOG_SUCCESS("Buffer created successfully!");
}

void Graphics::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_pTransferCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = nullptr;
	vkAllocateCommandBuffers(m_pLogicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_pTransferQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(m_pTransferQueue);

	vkFreeCommandBuffers(m_pLogicalDevice, m_pTransferCommandPool, 1, &commandBuffer);
}

void Graphics::CreateDescriptorSetLayout()
{
	LOG_INFO("Creating descriptor set layout...");

	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(m_pLogicalDevice, &layoutInfo, nullptr, &m_pDescriptorSetLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create descriptor set layout!");

	LOG_SUCCESS("Successfully created descriptor set layout!");
}

void Graphics::CreateUniformBuffers()
{
	LOG_INFO("Creating uniform buffers...");

	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	m_pUniformBuffers = std::make_unique<std::vector<VkBuffer>>(cMaxFramesInFlight);
	m_pUniformBuffersMemory = std::make_unique<std::vector<VkDeviceMemory>>(cMaxFramesInFlight);
	m_pUniformBuffersMapped = std::make_unique<std::vector<void*>>(cMaxFramesInFlight);

	for (size_t i = 0; i < cMaxFramesInFlight; i++)
	{
		CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_pUniformBuffers->data()[i], m_pUniformBuffersMemory->data()[i]);

		vkMapMemory(m_pLogicalDevice, m_pUniformBuffersMemory->data()[i], 0, bufferSize, 0, &m_pUniformBuffersMapped->data()[i]);
	}

	LOG_SUCCESS("Successfully created uniform buffers!");
}

void Graphics::UpdateUniformBuffer(uint32_t imageIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo = {};
	ubo.model = glm::rotate(glm::mat4(1.f), duration * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.proj = glm::perspective(glm::radians(45.f),
		static_cast<float>(m_SwapChainCapabilities.currentExtent.width) / static_cast<float>(m_SwapChainCapabilities.currentExtent.width),
		0.1f, 10.f);

	ubo.proj[1][1] *= -1;

	memcpy(m_pUniformBuffersMapped->data()[imageIndex], &ubo, sizeof(ubo));
}

void Graphics::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(cMaxFramesInFlight);

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	poolInfo.maxSets = static_cast<uint32_t>(cMaxFramesInFlight);

	if (vkCreateDescriptorPool(m_pLogicalDevice, &poolInfo, nullptr, &m_pDescriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create descriptor pool!");
}

void Graphics::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(cMaxFramesInFlight, m_pDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_pDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(cMaxFramesInFlight);
	allocInfo.pSetLayouts = layouts.data();

	m_pDescriptorSets = std::make_unique<std::vector<VkDescriptorSet>>(cMaxFramesInFlight);

	if (vkAllocateDescriptorSets(m_pLogicalDevice, &allocInfo, m_pDescriptorSets->data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate descriptor sets!");

	for (size_t i = 0; i < cMaxFramesInFlight; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_pUniformBuffers->data()[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = m_pDescriptorSets->data()[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;

		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = nullptr;
		descriptorWrite.pTexelBufferView = nullptr; 

		vkUpdateDescriptorSets(m_pLogicalDevice, 1, &descriptorWrite, 0, nullptr);
	}
}

void Graphics::CreateWindowSurface()
{
	LOG_INFO("Creating window surface...");

	if (glfwCreateWindowSurface(m_pVulkanInstance, m_pWindow, nullptr, &m_pWindowSurface) != VK_SUCCESS)
		throw std::runtime_error("failed to create window surface!");

	LOG_SUCCESS("Window surface created successfully!");
}

bool Graphics::IsDeviceSuitable(const VkPhysicalDevice& device)
{
	LOG_INFO("Checking if device is suitable...");

	LOG_DEBUG("Getting device's properties...");
	// get GPU properties
	vkGetPhysicalDeviceProperties(device, &m_PhysicalDeviceProperties);
	LOG_DEBUG(m_PhysicalDeviceProperties.apiVersion);
	LOG_DEBUG(m_PhysicalDeviceProperties.driverVersion);
	LOG_DEBUG(m_PhysicalDeviceProperties.vendorID);
	LOG_DEBUG(m_PhysicalDeviceProperties.deviceID);
	LOG_DEBUG(m_PhysicalDeviceProperties.deviceType);
	LOG_DEBUG(m_PhysicalDeviceProperties.deviceName);

	if (m_PhysicalDeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		LOG_WARNING("Device is not a discrete GPU, skipping device...");
		return false;
	}

	LOG_DEBUG("Geting device's max samples for MSAA:");
	// get max GPU samples for MSAA
	VkSampleCountFlags sampleCountFlags = m_PhysicalDeviceProperties.limits.framebufferColorSampleCounts & m_PhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (sampleCountFlags & VK_SAMPLE_COUNT_64_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_64_BIT;
	else if (sampleCountFlags & VK_SAMPLE_COUNT_32_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_32_BIT;
	else if (sampleCountFlags & VK_SAMPLE_COUNT_16_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_16_BIT;
	else if (sampleCountFlags & VK_SAMPLE_COUNT_8_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_8_BIT;
	else if (sampleCountFlags & VK_SAMPLE_COUNT_4_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_4_BIT;
	else if (sampleCountFlags & VK_SAMPLE_COUNT_2_BIT)
		m_eMaxMSAA = VK_SAMPLE_COUNT_2_BIT;
	LOG_DEBUG(m_eMaxMSAA);

	LOG_DEBUG("Getting device's features...");
	// get GPU features
	vkGetPhysicalDeviceFeatures(device, &m_PhysicalDeviceFeatures);
	LOG_DEBUG(m_PhysicalDeviceFeatures.robustBufferAccess);
	LOG_DEBUG(m_PhysicalDeviceFeatures.fullDrawIndexUint32);
	LOG_DEBUG(m_PhysicalDeviceFeatures.imageCubeArray);
	LOG_DEBUG(m_PhysicalDeviceFeatures.independentBlend);
	LOG_DEBUG(m_PhysicalDeviceFeatures.geometryShader);
	LOG_DEBUG(m_PhysicalDeviceFeatures.tessellationShader);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sampleRateShading);
	LOG_DEBUG(m_PhysicalDeviceFeatures.dualSrcBlend);
	LOG_DEBUG(m_PhysicalDeviceFeatures.logicOp);
	LOG_DEBUG(m_PhysicalDeviceFeatures.multiDrawIndirect);
	LOG_DEBUG(m_PhysicalDeviceFeatures.drawIndirectFirstInstance);
	LOG_DEBUG(m_PhysicalDeviceFeatures.depthClamp);
	LOG_DEBUG(m_PhysicalDeviceFeatures.depthBiasClamp);
	LOG_DEBUG(m_PhysicalDeviceFeatures.fillModeNonSolid);
	LOG_DEBUG(m_PhysicalDeviceFeatures.depthBounds);
	LOG_DEBUG(m_PhysicalDeviceFeatures.wideLines);
	LOG_DEBUG(m_PhysicalDeviceFeatures.largePoints);
	LOG_DEBUG(m_PhysicalDeviceFeatures.alphaToOne);
	LOG_DEBUG(m_PhysicalDeviceFeatures.multiViewport);
	LOG_DEBUG(m_PhysicalDeviceFeatures.samplerAnisotropy);
	LOG_DEBUG(m_PhysicalDeviceFeatures.textureCompressionETC2);
	LOG_DEBUG(m_PhysicalDeviceFeatures.textureCompressionASTC_LDR);
	LOG_DEBUG(m_PhysicalDeviceFeatures.textureCompressionBC);
	LOG_DEBUG(m_PhysicalDeviceFeatures.occlusionQueryPrecise);
	LOG_DEBUG(m_PhysicalDeviceFeatures.pipelineStatisticsQuery);
	LOG_DEBUG(m_PhysicalDeviceFeatures.vertexPipelineStoresAndAtomics);
	LOG_DEBUG(m_PhysicalDeviceFeatures.fragmentStoresAndAtomics);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderTessellationAndGeometryPointSize);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderImageGatherExtended);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageImageExtendedFormats);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageImageMultisample);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageImageReadWithoutFormat);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageImageWriteWithoutFormat);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderUniformBufferArrayDynamicIndexing);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderSampledImageArrayDynamicIndexing);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageBufferArrayDynamicIndexing);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderStorageImageArrayDynamicIndexing);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderClipDistance);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderCullDistance);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderFloat64);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderInt64);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderInt16);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderResourceResidency);
	LOG_DEBUG(m_PhysicalDeviceFeatures.shaderResourceMinLod);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseBinding);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidencyBuffer);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidencyImage2D);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidencyImage3D);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidency2Samples);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidency4Samples);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidency8Samples);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidency16Samples);
	LOG_DEBUG(m_PhysicalDeviceFeatures.sparseResidencyAliased);
	LOG_DEBUG(m_PhysicalDeviceFeatures.variableMultisampleRate);
	LOG_DEBUG(m_PhysicalDeviceFeatures.inheritedQueries);

	if (!m_PhysicalDeviceFeatures.geometryShader)
	{
		LOG_WARNING("Device does not support geometry shaders, skipping device...");
		return false;
	}

	// get device queue families & check if a queue family contain present and graphics support
	// find a queue family that supports both graphics and present operations for best performance
	// otherwise, we would need to create two separate queues, one for graphics and one for present
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	if (!queueFamilyCount)
	{
		LOG_WARNING("No queue families found for the device, skipping device...");
		return false;
	}

	auto queueFamilies = std::make_unique<std::vector<VkQueueFamilyProperties>>(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies->data());
	LOG_DEBUG(queueFamilies->size());
	auto queueFamilyProperties = std::make_unique<std::vector<std::vector<bool>>>(queueFamilyCount);

	int i = 0;
	bool presentQueueFound = false, graphicsQueueFound = false, transferQueueFound = false;
	for (const auto& queueFamily : *queueFamilies)
	{
		std::vector<bool> queueFamiliesFound = { false, false, false }; // graphics, compute, present

		LOG_DEBUG(i);
		LOG_DEBUG(queueFamily.queueCount);

		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamiliesFound[0] = true; // has graphics support
			graphicsQueueFound = true;
			m_uiDeviceGraphicsIndex = i;
			LOG_DEBUG("VK_QUEUE_GRAPHICS_BIT found!");
		}

		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			queueFamiliesFound[1] = true; // has graphics support
			transferQueueFound = true;
			m_uiDeviceTransferIndex = i;
			LOG_DEBUG("VK_QUEUE_TRANSFER_BIT found!");
		}

		VkBool32 windowPresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_pWindowSurface, &windowPresentSupport);
		if (windowPresentSupport)
		{
			queueFamiliesFound[2] = true; // has present support
			presentQueueFound = true;
			m_uiDevicePresentIndex = i;
			LOG_DEBUG("Surface Support found!");
		}

		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
			LOG_DEBUG("VK_QUEUE_COMPUTE_BIT found!");

		if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
			LOG_DEBUG("VK_QUEUE_SPARSE_BINDING_BIT found!");

		if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT)
			LOG_DEBUG("VK_QUEUE_PROTECTED_BIT found!");

		if (queueFamily.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
			LOG_DEBUG("VK_QUEUE_VIDEO_DECODE_BIT_KHR found!");

		if (queueFamily.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
			LOG_DEBUG("VK_QUEUE_VIDEO_ENCODE_BIT_KHR found!");

		if (queueFamily.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
			LOG_DEBUG("VK_QUEUE_OPTICAL_FLOW_BIT_NV found!");

		if (queueFamily.queueFlags & VK_QUEUE_FLAG_BITS_MAX_ENUM)
			LOG_DEBUG("VK_QUEUE_FLAG_BITS_MAX_ENUM found!");
		

		queueFamilyProperties->data()[i++] = queueFamiliesFound;
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
	for (const auto& familySupport : *queueFamilyProperties)
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

	m_pPhysicalDeviceQueueFamiliesProperties.swap(queueFamilies);

	// get device extensions & check if required extensions are supported
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	if (!extensionCount)
	{
		LOG_WARNING("No device extensions found, skipping device...");
		return false;
	}
	auto availableExtensions = std::make_unique<std::vector<VkExtensionProperties>>(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions->data());

	std::set<std::string> requiredExtensions(cRequiredDeviceExtensions.begin(), cRequiredDeviceExtensions.end());
	for (const auto& extension : *availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	if (!requiredExtensions.empty())
	{
		LOG_WARNING("Required device extensions not found:");
		LOG_WARNING(std::vector<std::string>(requiredExtensions.begin(), requiredExtensions.end()));
		LOG_WARNING("Skipping device...");
		return false;
	}
	m_pPhysicalDeviceExtensions.swap(availableExtensions);

	// Get device swapchain color formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_pWindowSurface, &formatCount, nullptr);
	if (!formatCount)
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}
	auto formats = std::make_unique<std::vector<VkSurfaceFormatKHR>>(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_pWindowSurface, &formatCount, formats->data());
	if (formats->empty())
	{
		LOG_WARNING("No surface formats found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_pSwapChainFormats.swap(formats);

	// Get device swapchain present modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_pWindowSurface, &presentModeCount, nullptr);
	if (!presentModeCount)
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}
	auto presentModes = std::make_unique<std::vector<VkPresentModeKHR>>(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_pWindowSurface, &presentModeCount, presentModes->data());
	if (presentModes->empty())
	{
		LOG_WARNING("No present modes found for the window surface (swap chain), skipping device...");
		return false;
	}
	m_pSwapChainPresentModes.swap(presentModes);

	// Get device swapchain surface capabilities for swap extent (resolution) and image count
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_pWindowSurface, &m_SwapChainCapabilities);
	
	// Get device's memory properties
	vkGetPhysicalDeviceMemoryProperties(device, &m_PhysicalDeviceMemoryProperties);

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
	LOG_DEBUG(m_pWindow);
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

	for (size_t i = 0; i < cMaxFramesInFlight; i++)
	{
		vkDestroyBuffer(m_pLogicalDevice, m_pUniformBuffers->data()[i], nullptr);
		vkFreeMemory(m_pLogicalDevice, m_pUniformBuffersMemory->data()[i], nullptr);
	}

	vkDestroyDescriptorPool(m_pLogicalDevice, m_pDescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(m_pLogicalDevice, m_pDescriptorSetLayout, nullptr);

	vkDestroyBuffer(m_pLogicalDevice, m_pIndexBuffer, nullptr);
	vkFreeMemory(m_pLogicalDevice, m_pIndexBufferMemory, nullptr);

	vkDestroyBuffer(m_pLogicalDevice, m_pVertexBuffer, nullptr);
	vkFreeMemory(m_pLogicalDevice, m_pVertexBufferMemory, nullptr);

	vkDestroyPipeline(m_pLogicalDevice, m_pGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_pLogicalDevice, m_pPipelineLayout, nullptr);

	vkDestroyRenderPass(m_pLogicalDevice, m_pRenderPass, nullptr);

	for (auto i = 0; i < m_pRenderFinishedSemaphores->size(); i++)
		if (m_pRenderFinishedSemaphores->data()[i] != nullptr)
			vkDestroySemaphore(m_pLogicalDevice, m_pRenderFinishedSemaphores->data()[i], nullptr);

	for (auto i = 0; i < cMaxFramesInFlight; i++)
	{
		if (m_pFrameInFlightFences->data()[i] != nullptr)
			vkDestroyFence(m_pLogicalDevice, m_pFrameInFlightFences->data()[i], nullptr);
		if (m_pImageAvailableSemaphores->data()[i] != nullptr)
			vkDestroySemaphore(m_pLogicalDevice, m_pImageAvailableSemaphores->data()[i], nullptr);
	}

	vkDestroyCommandPool(m_pLogicalDevice, m_pDrawingCommandPool, nullptr);
	vkDestroyCommandPool(m_pLogicalDevice, m_pTransferCommandPool, nullptr);

	vkDestroyDevice(m_pLogicalDevice, nullptr);

#ifdef _DEBUG
	CleanupDebugger();
#endif // _DEBUG

	vkDestroySurfaceKHR(m_pVulkanInstance, m_pWindowSurface, nullptr);
	vkDestroyInstance(m_pVulkanInstance, nullptr);

	glfwDestroyWindow(m_pWindow);

	glfwTerminate();

	LOG_SUCCESS("Vulkan cleaned up successfully!");
}

void Graphics::InitializeDebugger()
{
	LOG_INFO("Initializing Vulkan debugger...");

	auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pVulkanInstance, "vkCreateDebugUtilsMessengerEXT");

	if (vkCreateDebugUtilsMessengerEXT == nullptr)
		throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");


	if (vkCreateDebugUtilsMessengerEXT(m_pVulkanInstance, &m_DebugCreateInfo, nullptr, &m_pDebugMessenger) != VK_SUCCESS)
		throw std::runtime_error("failed to setup debugger");

	LOG_SUCCESS("Vulkan debugger initialized successfully!");
}

void Graphics::CleanupDebugger()
{
	LOG_INFO("Cleaning up Vulkan debugger...");
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pVulkanInstance, "vkDestroyDebugUtilsMessengerEXT");

	if (func == nullptr)
		throw std::runtime_error("vkCreateDebugUtilsMessengerEXT error");


	func(m_pVulkanInstance, m_pDebugMessenger, nullptr);

	LOG_SUCCESS("Vulkan debugger cleaned up successfully!");
}

bool Graphics::IsDebugSupported()
{
	LOG_INFO("Checking if Vulkan debugging is supported...");

	for (const auto& layerName : cRequiredValidationLayers)
		for (const auto& layer : *m_pInstanceLayers)
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

VKAPI_ATTR VkBool32 VKAPI_CALL Graphics::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	switch (messageSeverity)
	{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			LOG_DEBUG(std::string(pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			LOG_INFO(std::string(pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			LOG_WARNING(std::string(pCallbackData->pMessage));
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			LOG_FATAL(std::string(pCallbackData->pMessage));
			break;
		default:
			LOG_DEBUG(std::string(pCallbackData->pMessage));
			break;
	}
	return VK_FALSE;
}