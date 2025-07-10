#include "cGraphics.h"

std::shared_ptr<cGraphics> cGraphics::_instance = nullptr;

std::shared_ptr<cGraphics> cGraphics::Get()
{
    if (_instance == nullptr)
        _instance = std::make_shared<cGraphics>();

    return _instance;
}

cGraphics::cGraphics()
{
	InitializeWindow("Lordi Engine", 1280, 720);
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
}

cGraphics::~cGraphics()
{
#ifdef _DEBUG
    CleanupDebugger();
#endif // _DEBUG

    CleanupVulkan();
}

void cGraphics::CreateVulkanInstance()
{
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
    std::vector<const char*> newExtensions(ppExtensions, ppExtensions + extensionCount);

#ifdef _DEBUG
    newExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();

    m_DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    m_DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    m_DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    m_DebugCreateInfo.pfnUserCallback = DebugCallback;

    instanceCreateInfo.pNext = &m_DebugCreateInfo;
#endif // _DEBUG

    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(newExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = newExtensions.data();

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_pInstance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vulkan instance");
}

void cGraphics::GetVulkanData()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    auto extensions = std::make_unique<std::vector<VkExtensionProperties>>(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions->data());
    m_pExtensions.swap(extensions);

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    auto layers = std::make_unique<std::vector<VkLayerProperties>>(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers->data());
    m_pLayers.swap(layers);
}

void cGraphics::PickPhysicalDevice()
{
    auto isDeviceSuitable = [](cGraphics* ts, VkPhysicalDevice device) -> bool
    {
        // get GPU properties
        vkGetPhysicalDeviceProperties(device, &ts->m_DeviceProperties);
        // get GPU features
        vkGetPhysicalDeviceFeatures(device, &ts->m_DeviceFeatures);

        // get GPU queue families (indicies)
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        auto queueFamilies = std::make_unique<std::vector<VkQueueFamilyProperties>>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies->data());

        // check if queue families contain present and graphics support
        bool hasPresentQueue = false;
        bool hasGraphicsQueue = false;  
        int i = 0;
        for (const auto& queueFamily : *queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                hasGraphicsQueue = true;
                ts->m_uiGraphicsQueueFamilyIndex = i;
            }

            VkBool32 windowPresentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, ts->m_pWindowSurface, &windowPresentSupport);
            if (windowPresentSupport)
            {
                hasPresentQueue = true;
                ts->m_uiWindowPresentFamilyIndex = i;
            }

            i++;
        }

        if (!hasGraphicsQueue || !hasPresentQueue)
            return false;

        // get GPU extensions
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        auto availableExtensions = std::make_unique<std::vector<VkExtensionProperties>>(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions->data());

        std::set<std::string> requiredExtensions(ts->requiredDeviceExtensions.begin(), ts->requiredDeviceExtensions.end());

        for (const auto& extension : *availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        if (ts->m_DeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            !ts->m_DeviceFeatures.geometryShader || !requiredExtensions.empty())
            return false;

        ts->m_pQueueFamilies.swap(queueFamilies);
        ts->m_pDeviceExtensions.swap(availableExtensions);

        return true;
    };

    unsigned int deviceCount = 0;
    vkEnumeratePhysicalDevices(m_pInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("No GPU found with Vulkan support");
        return;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_pInstance, &deviceCount, devices.data());

    for (const auto& device : devices)
        if (isDeviceSuitable(this, device))
        {
            m_pPhysicalDevice = device;
            break;
        }

    if (m_pPhysicalDevice == nullptr)
        throw std::runtime_error("Failed to find a suitable GPU");
}

void cGraphics::CreateLogicalDevice()
{
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

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    deviceCreateInfo.enabledLayerCount = 0;

#ifdef _DEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
#endif // _DEBUG

    if (vkCreateDevice(m_pPhysicalDevice, &deviceCreateInfo, nullptr, &m_pLogicalDevice) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
        return;
    }

    vkGetDeviceQueue(m_pLogicalDevice, m_uiGraphicsQueueFamilyIndex, 0, &m_pGraphicsQueue);
    vkGetDeviceQueue(m_pLogicalDevice, m_uiWindowPresentFamilyIndex, 0, &m_pPresentQueue);
}

void cGraphics::CreateWindowSurface()
{
    if (glfwCreateWindowSurface(m_pInstance, m_pWindow, nullptr, &m_pWindowSurface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
}

bool cGraphics::IsLayerPresent(const std::string& layerName)
{
    if (m_pLayers == nullptr)
        GetVulkanData();

    for (const auto& layer : *m_pLayers)
    {
        if (!layerName.compare(layer.layerName))
            return true;
    }
    return false;
}

bool cGraphics::IsExtensionPresent(const std::string& extensionName)
{
    if (m_pExtensions == nullptr)
        GetVulkanData();

    for (const auto& extension : *m_pExtensions)
        if (!extensionName.compare(extension.extensionName))
            return true;

    return false;
}

void cGraphics::InitializeWindow(const std::string& windowName, const unsigned int& width, const unsigned int& height)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_pWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
}

void cGraphics::CleanupVulkan()
{
	if (m_pLogicalDevice != nullptr)
	    vkDestroyDevice(m_pLogicalDevice, nullptr);
	if (m_pWindow != nullptr)
		glfwDestroyWindow(m_pWindow);
    if (m_pWindowSurface != nullptr)
        vkDestroySurfaceKHR(m_pInstance, m_pWindowSurface, nullptr);
	if (m_pInstance != nullptr)
        vkDestroyInstance(m_pInstance, nullptr);

    glfwTerminate();
}

void cGraphics::InitializeDebugger()
{
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pInstance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT == nullptr)
    {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT");
        return;
    }

    if (vkCreateDebugUtilsMessengerEXT(m_pInstance, &m_DebugCreateInfo, nullptr, &m_pDebugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to setup debugger");
}

void cGraphics::CleanupDebugger()
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_pInstance, "vkDestroyDebugUtilsMessengerEXT");

    if (func == nullptr)
    {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT error");
        return;
    }

    func(m_pInstance, m_pDebugMessenger, nullptr);
}

bool cGraphics::IsDebugSupported()
{
    for (const auto& layerName : requiredValidationLayers)
        if (!IsLayerPresent(layerName))
            return false;

    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL cGraphics::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cout << pCallbackData->pMessage << '\n';

    return VK_FALSE;
}