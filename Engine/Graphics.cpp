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
#ifdef _DEBUG
    CleanupDebugger();
#endif // _DEBUG

    CleanupVulkan();
}

void Graphics::Initialize()
{
    LOG_INFO("Initializing Graphics...");

    InitializeWindow();
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
    CreateGraphicsPipeline();
    CreateFrameBuffers();

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
        throw std::runtime_error("No Vulkan extensions on this device found");

	std::unique_ptr<std::vector<const char*>> newExtensions = std::make_unique<std::vector<const char*>>(ppExtensions, ppExtensions + extensionCount);

#ifdef _DEBUG
    newExtensions->push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(cRequiredValidationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = cRequiredValidationLayers.data();

    m_DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    m_DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    m_DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    m_DebugCreateInfo.pfnUserCallback = DebugCallback;

    instanceCreateInfo.pNext = &m_DebugCreateInfo;
#endif // _DEBUG
    
    LOG_DEBUG(*newExtensions.get());

    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(newExtensions->size());
    instanceCreateInfo.ppEnabledExtensionNames = newExtensions->data();

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_pInstance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vulkan instance");

	LOG_SUCCESS("Vulkan instance created successfully!");
}

void Graphics::GetVulkanData()
{
	LOG_INFO("Getting Vulkan data...");

    uint32_t globalExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &globalExtensionCount, nullptr);
    if (!globalExtensionCount)
        throw std::runtime_error("No Vulkan extensions found");


    auto extensions = std::make_unique<std::vector<VkExtensionProperties>>(globalExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &globalExtensionCount, extensions->data());
    m_pGlobalExtensions.swap(extensions);

    auto globalExtensionNames = std::make_unique<std::vector<std::string>>(globalExtensionCount);
    int i = 0;
    for (const auto& extension : *m_pGlobalExtensions)
        globalExtensionNames->data()[i++] = std::string(extension.extensionName);
	LOG_DEBUG(*globalExtensionNames.get());

    uint32_t globalLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&globalLayerCount, nullptr);
    if (!globalLayerCount)
        throw std::runtime_error("No Vulkan layers found");


    auto layers = std::make_unique<std::vector<VkLayerProperties>>(globalLayerCount);
    vkEnumerateInstanceLayerProperties(&globalLayerCount, layers->data());
    m_pGlobalLayers.swap(layers);

	auto globalLayerNames = std::make_unique<std::vector<std::string>>(globalLayerCount);
    i = 0;
    for (const auto& layer : *m_pGlobalLayers)
        globalLayerNames->data()[i++] = std::string(layer.layerName);
	LOG_DEBUG(*globalLayerNames.get());

	LOG_SUCCESS("Vulkan data retrieved successfully!");
}

void Graphics::PickPhysicalDevice()
{
	LOG_INFO("Picking physical device (GPU)...");

    unsigned int deviceCount = 0;
    vkEnumeratePhysicalDevices(m_pInstance, &deviceCount, nullptr);
    if (deviceCount == 0)
        throw std::runtime_error("No device found with Vulkan support");

    LOG_DEBUG(deviceCount);

	auto m_Devices = std::make_unique<std::vector<VkPhysicalDevice>>(deviceCount);
    vkEnumeratePhysicalDevices(m_pInstance, &deviceCount, m_Devices->data());

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
    std::set<uint32_t> uniqueQueueFamilies = { m_uiGraphicsQueueFamilyIndex, m_uiWindowPresentFamilyIndex };

    // Queue family creation
	float queuePriority = 1.0f; // priority of the queue, higher values indicate higher priority (0.0f is lowest priority)
    for (auto queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // specifies the type of structure
        queueCreateInfo.queueFamilyIndex = queueFamily; // index of the queue family to create
        queueCreateInfo.queueCount = 1; // number of queues to create
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; // specifies the type of structure
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); // number of queue create infos
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data(); // pointer to the queue create infos
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures; // pointer to the enabled features

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(cRequiredDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = cRequiredDeviceExtensions.data();

    deviceCreateInfo.enabledLayerCount = 0;

#ifdef _DEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(cRequiredValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = cRequiredValidationLayers.data();
#endif // _DEBUG

    if (vkCreateDevice(m_pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pLogicalDevice) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");


    vkGetDeviceQueue(m_pLogicalDevice, m_uiGraphicsQueueFamilyIndex, 0, &m_pGraphicsQueue);
    vkGetDeviceQueue(m_pLogicalDevice, m_uiWindowPresentFamilyIndex, 0, &m_pPresentQueue);

	LOG_SUCCESS("Logical device created successfully!");
}

void Graphics::CreateSwapChain()
{
    LOG_INFO("Creating swap chain...");

    // format of the swap chain images, color channels and types
    VkSurfaceFormatKHR swapChainFormat = m_pSwapChainFormats->data()[0];
	// surface format specifications (color format and color space)
    for (const auto& format : *m_pSwapChainFormats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            swapChainFormat = format;
            break;
        }

    // surface present mode specifications (how images are presented to the surface) (VSync)
    VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    
	auto settings = Settings::Get()->GetGraphicsSettings();
    VkPresentModeKHR userPresentMode = VK_PRESENT_MODE_FIFO_KHR; // default to FIFO (VSync)
    switch (settings.m_VSyncOption)
    {
        case VSyncOptions::NONE:
            userPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // No VSync
            break;
        case VSyncOptions::ENABLED:
            userPresentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync
            break;
        case VSyncOptions::RELAXED:
            userPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR; // Relaxed VSync
            break;
        case VSyncOptions::MAILBOX:
            userPresentMode = VK_PRESENT_MODE_MAILBOX_KHR; // Mailbox VSync (triple buffering)
            break;
    }
        
    for (const auto& presentMode : *m_pSwapChainPresentModes)
        if (presentMode == userPresentMode)
        {
            swapChainPresentMode = presentMode;
            break;
        }

    // resolution of the swap chain images, width and height
    VkExtent2D swapChainExtent = {};
	// surface swap extent specifications (resolution of the swap chain images)
    if (m_SwapChainCapabilities.currentExtent.width == UINT32_MAX)
    {
        int width, height;
		glfwGetFramebufferSize(m_pWindow, &width, &height);
        swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), m_SwapChainCapabilities.minImageExtent.width, m_SwapChainCapabilities.maxImageExtent.width);
        swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), m_SwapChainCapabilities.minImageExtent.height, m_SwapChainCapabilities.maxImageExtent.height);
    }
    else
    {
        // Use the current extent
        swapChainExtent = m_SwapChainCapabilities.currentExtent;
	}

	uint32_t swapChainImageCount = m_SwapChainCapabilities.minImageCount + 1;
    if (m_SwapChainCapabilities.maxImageCount > 0 && swapChainImageCount > m_SwapChainCapabilities.maxImageCount)
    {
        LOG_WARNING("Swap chain image count exceeds maximum, clamping to maximum");
		swapChainImageCount = m_SwapChainCapabilities.maxImageCount;
	}

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

    if (m_uiGraphicsQueueFamilyIndex != m_uiWindowPresentFamilyIndex)
    {
		LOG_WARNING("Graphics queue family is different from present queue family, using concurrent mode for swap chain images.");
        // If the graphics queue family is different from the present queue family, we need to specify the queue families
        uint32_t queueFamilyIndices[] = { m_uiGraphicsQueueFamilyIndex, m_uiWindowPresentFamilyIndex };
        m_SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // images can be shared between multiple queues
        m_SwapChainCreateInfo.queueFamilyIndexCount = 2; // number of queue families
        m_SwapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices; // pointer to the queue family indices
    }
    else
    {
		LOG_INFO("Graphics queue family is the same as present queue family, using exclusive mode for swap chain images.");
        // If the graphics queue family is the same as the present queue family, we can use exclusive mode
        m_SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // images are only used by one queue at a time
        m_SwapChainCreateInfo.queueFamilyIndexCount = 0; // no queue families
        m_SwapChainCreateInfo.pQueueFamilyIndices = nullptr; // no queue family indices
	}

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

    if (vkCreateSwapchainKHR(m_pLogicalDevice, &m_SwapChainCreateInfo, nullptr, &m_pSwapChain) != VK_SUCCESS)
		throw std::runtime_error("failed to create swap chain!");

    // Get swap chain images
    vkGetSwapchainImagesKHR(m_pLogicalDevice, m_pSwapChain, &swapChainImageCount, nullptr);
    if (!swapChainImageCount)
        throw std::runtime_error("No swap chain images found");

    auto swapChainImages = std::make_unique<std::vector<VkImage>>(swapChainImageCount);
    vkGetSwapchainImagesKHR(m_pLogicalDevice, m_pSwapChain, &swapChainImageCount, swapChainImages->data());
	m_pSwapChainImages.swap(swapChainImages);

	LOG_SUCCESS("Swap chain created successfully!");
}

void Graphics::CreateImageViews()
{
    LOG_INFO("Creating image views for swap chain images...");

	m_pSwapChainImageViews = std::make_unique<std::vector<VkImageView>>(m_pSwapChainImages->size());

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
			throw std::runtime_error("failed to create image view for swap chain image!");
    }

	LOG_SUCCESS("Image views for swap chain images created successfully!");
}

void Graphics::CreateRenderPass()
{
    LOG_INFO("Creating render pass...");

    // Render pass is a collection of attachments (buffers) and subpasses (rendering operations)
    // It defines how images are used during rendering
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_SwapChainCreateInfo.imageFormat; // format of the color attachment
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // number of samples per pixel (1 for no multisampling)

    // what to do with data in attachment before rendering
    // VK_ATTACHMENT_LOAD_OP_LOAD (preserves  data in attachment)
    // VK_ATTACHMENT_LOAD_OP_CLEAR (clears data in attachment)
    // VK_ATTACHMENT_LOAD_OP_DONT_CARE (ignores the data in the attachment)
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear the attachment before rendering

	// what to do with data in attachment after rendering
	// VK_ATTACHMENT_STORE_OP_STORE stores the data in the attachment in memory to be used later
	// VK_ATTACHMENT_STORE_OP_DONT_CARE ignores the data in the attachment, data is not preserved
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // store the attachment after rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // no stencil buffer
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // no stencil buffer

    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    //  -   images used as color attachments during rendering
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    //  -   images used for presentation to the screen aka for the swap chain)
	// VK_IMAGE_LAYOUT_UNDEFINED 
    //  -   dont care about initial layout, we'll set it in finalLayout, also data not guaranteed to be preserved
    // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 
    //  -   images used for transfer operations (e.g. blitting, copying, etc.)
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // initial layout of the attachment
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // final layout of the attachment (ready for presentation)

	// Attachment reference is used to specify how the attachment is used in the subpass
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0; // index of the color attachment
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout of color attachment during rendering

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
    subpass.colorAttachmentCount = 1; // number of color attachments
    subpass.pColorAttachments = &colorAttachmentRef; // pointer to the color attachment reference
	// colorAttachmentRef & subpass = "layout(location = 0) out vec4 outColor" in the fragment shader

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1; // number of attachments
    renderPassInfo.pAttachments = &colorAttachment; // pointer to the attachments
    renderPassInfo.subpassCount = 1; // number of subpasses
	renderPassInfo.pSubpasses = &subpass; // pointer

	if (vkCreateRenderPass(m_pLogicalDevice, &renderPassInfo, nullptr, &m_pRenderPass) != VK_SUCCESS)
		throw std::runtime_error("failed to create render pass!");

	LOG_SUCCESS("Render pass created successfully!");
}

void Graphics::CreateGraphicsPipeline()
{
	LOG_INFO("Creating graphics pipeline...");

	// read the compiled shader byte code from files
	auto fragShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/frag.spv");
	auto vertShaderCode = Utility::ReadFile("Engine/Shaders/Compiled/vert.spv");

	// create shader modules from the byte code
	VkShaderModuleCreateInfo fragShaderInfo = {};
	fragShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragShaderInfo.codeSize = fragShaderCode.size();
	fragShaderInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

	if (vkCreateShaderModule(m_pLogicalDevice, &fragShaderInfo, nullptr, &m_pFragmentShaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create fragment shader module!");

	VkShaderModuleCreateInfo vertShaderInfo = {};
	vertShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderInfo.codeSize = vertShaderCode.size();
	vertShaderInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

    if (vkCreateShaderModule(m_pLogicalDevice, &vertShaderInfo, nullptr, &m_pVertexShaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create vertex shader module!");

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

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
	
    // dynamic states are states that can be changed at runtime
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, // viewport can be changed at runtime
        VK_DYNAMIC_STATE_SCISSOR // scissor can be changed at runtime
    };
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// vertex input state describes the format of the vertex data
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	// input assembly state describes how the vertex data is assembled into primitives
	// primitives are the basic building blocks of the graphics pipeline, e.g. points, lines, triangles
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

	// viewport specifications (region of framebuffer to render to)
    VkViewport viewport = {};
	viewport.x = 0.0f; // x position of the viewport
	viewport.y = 0.0f; // y position of the viewport
	viewport.width = static_cast<float>(m_SwapChainCreateInfo.imageExtent.width); // width of the viewport
	viewport.height = static_cast<float>(m_SwapChainCreateInfo.imageExtent.height); // height of the viewport
	viewport.minDepth = 0.0f; // minimum depth of the viewport
	viewport.maxDepth = 1.0f; // maximum depth of the viewport

	// scissor specifications (region of framebuffer to keep, used for clipping)
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 }; // offset of the scissor rectangle
	scissor.extent = m_SwapChainCreateInfo.imageExtent; // size of the scissor rectangle

	// viewport state describes the viewport and scissor rectangles, since dynamic states are used, we only need to specify the count
	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1; // number of viewports
	viewportStateInfo.scissorCount = 1; // number of scissors

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
	rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL; // fill mode
    // line width in fragments
	// TO-DO: if thicker than 1.0f, a GPU feature (wideLines) needs to be enabled
	rasterizerInfo.lineWidth = 1.0f; // line width
	// specifies how the triangles are culled, e.g.
	// VK_CULL_MODE_NONE for no culling,
	// VK_CULL_MODE_FRONT_BIT for front face culling,
	// VK_CULL_MODE_BACK_BIT for back face culling, and
	// VK_CULL_MODE_FRONT_AND_BACK for both front and back face culling
	rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT; // back face culling
	// specifies the front face of the triangles, e.g.
	// VK_FRONT_FACE_COUNTER_CLOCKWISE for counter-clockwise front face,
	// VK_FRONT_FACE_CLOCKWISE for clockwise front face
	rasterizerInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // counter-clockwise front face
	// specifies if depth bias is enabled, used for shadow mapping to avoid shadow acne
	rasterizerInfo.depthBiasEnable = VK_FALSE; // depth bias is disabled
	rasterizerInfo.depthBiasConstantFactor = 0.0f; // constant factor for depth bias
	rasterizerInfo.depthBiasClamp = 0.0f; // clamp for depth bias
	rasterizerInfo.depthBiasSlopeFactor = 0.0f; // slope factor for depth bias

	// multisampling state describes how the fragments are sampled, e.g. anti-aliasing
	// multisampling is used to reduce aliasing artifacts in the rendered image
	// works by sampling multiple fragments per pixel and averaging the results
	// TO-DO: If multisampling is enabled, GPU feature (sampleRateShading) needs to be enabled
    VkPipelineMultisampleStateCreateInfo multisamplingInfo = {};
    multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingInfo.sampleShadingEnable = VK_FALSE;
    multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisamplingInfo.minSampleShading = 1.0f;
    multisamplingInfo.pSampleMask = nullptr;
    multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
    multisamplingInfo.alphaToOneEnable = VK_FALSE;

	// depth stencil state describes how the depth and stencil buffers are used
	// VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};

	auto settings = Settings::Get()->GetGraphicsSettings();

	// color blending state describes how the colors of the fragments are blended with the colors of the framebuffer.
	// attachment state describes how the color attachment is blended for each framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = settings.m_ColorBlendingOption == ColorBlendingOptions::MIX ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// global color blending settings
    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {};
    colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateInfo.logicOpEnable = settings.m_ColorBlendingOption == ColorBlendingOptions::BITWISE ? VK_TRUE : VK_FALSE;
    colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateInfo.attachmentCount = 1;
    colorBlendStateInfo.pAttachments = &colorBlendAttachment;
    colorBlendStateInfo.blendConstants[0] = 0.0f;
    colorBlendStateInfo.blendConstants[1] = 0.0f;
    colorBlendStateInfo.blendConstants[2] = 0.0f;
    colorBlendStateInfo.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // no descriptor sets
	pipelineLayoutInfo.pSetLayouts = nullptr; // no descriptor sets
	pipelineLayoutInfo.pushConstantRangeCount = 0; // no push constants
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // no push constants

	if (vkCreatePipelineLayout(m_pLogicalDevice, &pipelineLayoutInfo, nullptr, &m_pPipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");
    
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

	if (vkCreateGraphicsPipelines(m_pLogicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pGraphicsPipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

    // cleanup shader modules
	vkDestroyShaderModule(m_pLogicalDevice, m_pFragmentShaderModule, nullptr);
	vkDestroyShaderModule(m_pLogicalDevice, m_pVertexShaderModule, nullptr);

	LOG_SUCCESS("Graphics pipeline created successfully!");
}

void Graphics::CreateFrameBuffers()
{
    LOG_INFO("Creating frame buffers...");

    auto frameBuffers = std::make_unique<std::vector<VkFramebuffer>>(m_pSwapChainImageViews->size());

    for (auto i = 0; i < m_pSwapChainImageViews->size(); i++)
    {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_pRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_pSwapChainImageViews->data()[i];
        framebufferInfo.width = m_SwapChainCreateInfo.imageExtent.width;
        framebufferInfo.height = m_SwapChainCreateInfo.imageExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_pLogicalDevice, &framebufferInfo, nullptr, &frameBuffers->data()[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }

	m_pFrameBuffers.swap(frameBuffers);

	LOG_SUCCESS("Frame buffers created successfully!");
}

void Graphics::CreateWindowSurface()
{
	LOG_INFO("Creating window surface...");

    if (glfwCreateWindowSurface(m_pInstance, m_pWindow, nullptr, &m_pWindowSurface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");

	LOG_SUCCESS("Window surface created successfully!");
}

bool Graphics::IsLayerPresent(const std::string& layerName)
{
    if (m_pGlobalLayers == nullptr)
        GetVulkanData();

    for (const auto& layer : *m_pGlobalLayers)
    {
        if (!layerName.compare(layer.layerName))
            return true;
    }
    return false;
}

bool Graphics::IsExtensionPresent(const std::string& extensionName)
{
    if (m_pGlobalExtensions == nullptr)
        GetVulkanData();

    for (const auto& extension : *m_pGlobalExtensions)
        if (!extensionName.compare(extension.extensionName))
            return true;

    return false;
}

bool Graphics::IsDeviceSuitable(const VkPhysicalDevice& device)
{
	LOG_INFO("Checking if device is suitable...");

    // get GPU properties
    vkGetPhysicalDeviceProperties(device, &m_DeviceProperties);
    if (m_DeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
		LOG_WARNING("Device is not a discrete GPU, skipping device...");
		return false;
    }

    // get GPU features
    vkGetPhysicalDeviceFeatures(device, &m_DeviceFeatures);
    if (!m_DeviceFeatures.geometryShader)
    {
        LOG_WARNING("Device does not support geometry shaders, skipping device...");
		return false;
    }

    // get GPU queue families & check if queue families contain present and graphics support
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

	auto queueFamilyProperties = std::make_unique<std::vector<std::pair<bool, bool>>>(queueFamilyCount);
    
    bool presentQueueFound = false;
    bool graphicsQueueFound = false;
    int i = 0;
    for (const auto& queueFamily : *queueFamilies)
    {
		std::pair<bool, bool> familySupport = { false, false };

        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
			familySupport.first = true; // has graphics support
            graphicsQueueFound = true;
            m_uiGraphicsQueueFamilyIndex = i;
        }

        VkBool32 windowPresentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_pWindowSurface, &windowPresentSupport);
        if (windowPresentSupport)
        {
			familySupport.second = true; // has present support
            presentQueueFound = true;
            m_uiWindowPresentFamilyIndex = i;
        }
        
		queueFamilyProperties->data()[i++] = familySupport;
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

	// Check if there is a queue family that supports both graphics and present operations (best performance)
    i = 0;
    for (const auto& familySupport : *queueFamilyProperties)
    {
        if (familySupport.first && familySupport.second)
        {
            m_uiGraphicsQueueFamilyIndex = i;
            m_uiWindowPresentFamilyIndex = i;
            break;
        }
        i++;
	}

    m_pQueueFamilies.swap(queueFamilies);

    // get GPU extensions & check if required extensions are supported on GPU
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    if (!extensionCount)
    {
        LOG_WARNING("No device extensions found for the GPU, skipping device...");
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
    m_pDeviceExtensions.swap(availableExtensions);

    // Get GPU swapchain color formats
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

	// Get GPU swapchain present modes
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

    // Get GPU swapchain surface capabilities for swap extent (resolution) and image count
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_pWindowSurface, &m_SwapChainCapabilities);

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

    m_pWindow = glfwCreateWindow(settings.m_uiWindowWidth, settings.m_uiWindowHeight, settings.m_sWindowName.c_str(), nullptr, nullptr);
    if (m_pWindow == nullptr)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
	}
	LOG_SUCCESS("GLFW window created successfully");
    LOG_DEBUG(m_pWindow);
}

void Graphics::CleanupVulkan()
{
	LOG_INFO("Cleaning up Vulkan...");

	for (const auto& framebuffer : *m_pFrameBuffers)
		vkDestroyFramebuffer(m_pLogicalDevice, framebuffer, nullptr);
    
	if (m_pGraphicsPipeline != nullptr)
		vkDestroyPipeline(m_pLogicalDevice, m_pGraphicsPipeline, nullptr);

	if (m_pPipelineLayout != nullptr)
		vkDestroyPipelineLayout(m_pLogicalDevice, m_pPipelineLayout, nullptr);

	if (m_pRenderPass != nullptr)
		vkDestroyRenderPass(m_pLogicalDevice, m_pRenderPass, nullptr);

    for (const auto& imageView : *m_pSwapChainImageViews)
		vkDestroyImageView(m_pLogicalDevice, imageView, nullptr);

    if (m_pSwapChain != nullptr)
        vkDestroySwapchainKHR(m_pLogicalDevice, m_pSwapChain, nullptr);

	if (m_pLogicalDevice != nullptr)
	    vkDestroyDevice(m_pLogicalDevice, nullptr);

	if (m_pWindow != nullptr)
		glfwDestroyWindow(m_pWindow);

    if (m_pWindowSurface != nullptr)
        vkDestroySurfaceKHR(m_pInstance, m_pWindowSurface, nullptr);

	if (m_pInstance != nullptr)
        vkDestroyInstance(m_pInstance, nullptr);

    glfwTerminate();

	LOG_SUCCESS("Vulkan cleaned up successfully!");
}

void Graphics::InitializeDebugger()
{
	LOG_INFO("Initializing Vulkan debugger...");

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pInstance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT == nullptr)
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT failed");


    if (vkCreateDebugUtilsMessengerEXT(m_pInstance, &m_DebugCreateInfo, nullptr, &m_pDebugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to setup debugger");

	LOG_SUCCESS("Vulkan debugger initialized successfully!");
}

void Graphics::CleanupDebugger()
{
	LOG_INFO("Cleaning up Vulkan debugger...");
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pInstance, "vkDestroyDebugUtilsMessengerEXT");

    if (func == nullptr)
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT error");


    func(m_pInstance, m_pDebugMessenger, nullptr);

	LOG_SUCCESS("Vulkan debugger cleaned up successfully!");
}

bool Graphics::IsDebugSupported()
{
	LOG_INFO("Checking if debug is supported...");
    for (const auto& layerName : cRequiredValidationLayers)
        if (!IsLayerPresent(layerName))
        {
            LOG_FATAL("Layer not found: " + std::string(layerName));
            return false;
        }

	LOG_SUCCESS("Debug is supported!");
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Graphics::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
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