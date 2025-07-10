#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <vector>
#include <memory>
#include <set>

// Vulkan Graphics API: rendering
// GLFW: window management & input handling
// GLM: linear algebra

/*
	Steps to draw a triangle:

	[done!] 1. Initialize Vulkan, create a Vulkan instance, if debugging, enable for vulkan.

	[done!] 2. Get available physical devices (GPUs/etc) and select the best one.
		
		- Possible to use multiple devices at once, but for now just one. :P

		- Best is subjective, but we have a few messures to go off:

			- Availible queue families (commands availible to submit to the device)

			- Availible features and versions

			- Type of device (GPU or something else?)

			- Limitations of the device (memory, etc)

		- Physical devices are cleaned up when Vulkan's instance is destroyed

	[done!] 3. Create a logical device to interface with the physical device.

	[done!] 4. Create a graphics queue to submit rendering commands.

	 	- Operations in Vulkan are asynchronously (one by one) executed on the queue

	[done!] 5. Create a window using GLFW.

	[] 6. Create a surface for rendering.

	 	- Surface is cross-platform abstraction for rendering to a window or other displayable areah

	[] 7. Create a swap chain to manage the images that will be presented to the surface.
		
		- Involves checking for swap chain support on the GPU

	[] 8. Create image views for the swap chain images.

	 	- Wrap the swap chain into image views to be drawn to the screen.

	 	- Image views specify what part of the image to use.

	[] 9. Create a framebuffer.

	 	- Used for referencing image views for color, depth, and stencil attachments.

	[] 10 Create a render pass to define how the images will be rendered.

	 	- Descrive type of images used, how they will be used, and how their contents should be handled during rendering.

	[] 11. Create a graphics pipeline to define how the triangle will be drawn.

	 	- Describes config of GPU, e.g. viewport size, shaders to use, input assembly, rasterization, etc.

	 	- Different shaders/vertlex layout require different pipelines.

	[] 12. Create a command pool to manage command buffers.

	 	- Command pool is used to allocate and free command buffers.

	[] 13. Create command buffers to record the drawing commands.

	 	- Queue operations are recordered first into command buffers, which are then submitted to the queue for execution.

	[] 14. Submit the command buffers to the graphics queue for execution.

	[] 15. Present the rendered images to the surface.

	[] 16. Cleanup resources when done.
*/

class cGraphics
{
	friend class cLordi;
	// instance of cGraphics, used as a singleton (single instance throughout the application)
	static std::shared_ptr<cGraphics> _instance;
public:
	// returns the instance of cGraphics, creates it if it doesn't exist
	static std::shared_ptr<cGraphics> Get();

	cGraphics();
	~cGraphics();
private:
	bool IsDebugSupported();
	void InitializeDebugger();
	void CleanupDebugger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

	void InitializeWindow(const std::string& windowName, const unsigned int& width, const unsigned int& height);

	void CreateVulkanInstance();
	void CleanupVulkan();

	void GetVulkanData();
	bool IsLayerPresent(const std::string& layerName);
	bool IsExtensionPresent(const std::string& extensionName);

	// Pick a physical device (GPU) that supports the required features and queues
	void PickPhysicalDevice();
	// Create logical device, used to interface with the physical device
	// Describes the queues and features that the logical device will support
	void CreateLogicalDevice();

	void CreateWindowSurface();
private:
	const std::vector<const char*> requiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	
	GLFWwindow* m_pWindow = nullptr;

	VkDebugUtilsMessengerEXT m_pDebugMessenger = nullptr;
	VkDebugUtilsMessengerCreateInfoEXT m_DebugCreateInfo = {};

	// Vulkan instance, the connection between the application and the Vulkan library
	VkInstance m_pInstance = nullptr;
	// Extensions are used to extend the functionality of Vulkan, each extension provides additional features or capabilities
	// These are the ones available on the system
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pExtensions = nullptr;
	// Validation layers (debugging functionality)
	std::unique_ptr<std::vector<VkLayerProperties>> m_pLayers = nullptr;

	// Physical device (GPU)
	VkPhysicalDevice m_pPhysicalDevice = nullptr; 

	// name, type, supported Vulkan version of GPU
	VkPhysicalDeviceProperties m_DeviceProperties = {};
	// optional features like texture compression, 64 bit floats and multi viewport rendering of GPU
	VkPhysicalDeviceFeatures m_DeviceFeatures = {};
	// extensions from GPU (essential features)
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pDeviceExtensions = nullptr;

	// Each family of queues allows a subset of commands.
	std::unique_ptr<std::vector<VkQueueFamilyProperties>> m_pQueueFamilies = nullptr;
	// Index of the graphics queue family
	uint32_t m_uiGraphicsQueueFamilyIndex = 0;
	// Index of queue that supports window surface presentation
	uint32_t m_uiWindowPresentFamilyIndex = 0;

	// Logical device is used to interface with the physical device
	VkDevice m_pLogicalDevice = nullptr;

	// Handle to our graphics queue to submit commands
	VkQueue m_pGraphicsQueue = nullptr;
	// Handle to our present queue for window surface drawing
	VkQueue m_pPresentQueue = nullptr;

	VkSurfaceKHR m_pWindowSurface = nullptr;
};