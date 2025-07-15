#include "Graphics.h"

std::shared_ptr<Graphics> Graphics::_instance = nullptr;

std::shared_ptr<Graphics> Graphics::Get()
{
    if (_instance == nullptr)
        _instance = std::shared_ptr<Graphics>(new Graphics());

    return _instance;
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

    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(c_RequiredValidationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = c_RequiredValidationLayers.data();

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

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(c_RequiredDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = c_RequiredDeviceExtensions.data();

    deviceCreateInfo.enabledLayerCount = 0;

#ifdef _DEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(c_RequiredValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = c_RequiredValidationLayers.data();
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

    // presentation mode of the swap chain
    VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    // surface present mode specifications (how images are presented to the surface) (VSync)
    VkPresentModeKHR userPresentMode = VK_PRESENT_MODE_FIFO_KHR; // default to FIFO (VSync)

	auto settings = Settings::Get()->GetGraphicsSettings();

    if (settings.Empty())
        LOG_WARNING("Graphics settings are empty, using default values...");
    else
        switch (settings.m_eVSyncOption)
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

	LOG_SUCCESS("Swap chain created successfully!");
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

    std::set<std::string> requiredExtensions(c_RequiredDeviceExtensions.begin(), c_RequiredDeviceExtensions.end());
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
    if (settings.Empty())
    {
        LOG_WARNING("Graphics settings are not set, using defaults...");
		settings = settings.Defaults();
	}

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
    for (const auto& layerName : c_RequiredValidationLayers)
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