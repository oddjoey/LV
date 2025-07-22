#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>		// std::cout
#include <vector>		// std::vector
#include <memory>		// std::shared_ptr, std::unique_ptr
#include <set>			// std::set
#include <algorithm>	// std::clamp

#include "Utility.h"
#include "Logger.h"
#include "Settings.h"

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

	[done!] 6. Create a surface for rendering.

	 		- Surface is cross-platform abstraction for rendering to a window or other displayable area

	[done!] 7. Create a swap chain with settings to manage the images that will be presented to the surface.
		
			- Involves checking for swap chain support on the GPU
			
				- Supported surface color formats (color channels and types) and surface color space (sRGB, linear, etc.)

					- We will use VK_FORMAT_B8G8R8A8_SRGB for color format and VK_COLOR_SPACE_SRGB_NONLINEAR_KHR for color space, 
					if they are available

						- VK_FORMAT_B8G8R8A8_SRGB is a common format for swap chain images, it has 8 bits per channel and is sRGB encoded

						- VK_COLOR_SPACE_SRGB_NONLINEAR_KHR is a common color space for swap chain images, it is non-linear and sRGB encoded

				- Supported presentation modes (e.g. FIFO, mailbox, etc.) aka VSync

					- We will use VSync settings provided by settings, if available, if not, 
					we will use VK_PRESENT_MODE_FIFO_KHR (guaranteed to be supported)

				- Supported swap extent (min/max image dimensions of swap chain) and image count (min/max number of images in the swap chain)

					- We will use the min/max image dimensions and image count provided by the GPU, if available, 
					if not, we will use the default values

			- We specify swap chain image count aswell.

	[done!] 8. Create image views for the swap chain images.

	 		- Wrap the swap chain into image views to be drawn to the screen.

	 		- Image views specify what part of the image to use.

				- e.g 2D texture, mipmap, cube map, etc.

	[done!] 9 Create a render pass to define how the images will be rendered.

	 		- A collection of rendering operations on attachments (like color and depth),
			defined in stages. It sets the structure for what happens each frame.

			- Subdivisions within a render pass that allow rendering in stages (like shadow pass, then lighting).
			They help optimize memory access.

	[done!] 10. Create a graphics pipeline to define how the triangle will be drawn.

			- The full configuration of shaders, states (blend, rasterization, etc.),
			and input/output formats that define how rendering happens

	 		- Describes config of GPU, e.g. viewport size, shaders to use, input assembly, rasterization, etc.

	 		- Different shaders/vertlex layout require different pipelines.

	[done!] 11. Create a framebuffer.

	 		- A collection of attachments (image views) that are used during a render pass to store output (color, depth, etc.).

	[done!] 12. Create a command pool to manage command buffers.

	 		- Command pool is used to allocate and free command buffers.

			- A memory manager for command buffers. It ensures allocation is efficient and thread-safe.

	[done!] 13. Create command buffers to record the drawing commands.

			- Sequences of GPU instructions (draw calls, binding resources) recorded once and submitted many times.

	 		- Queue operations are recordered first into command buffers, which are then submitted to the queue for execution.

		[] 14. Create synchronization objects (semaphores and fences) to synchronize the rendering process.

			- GPU-GPU sync primitives used to control the order of operations—e.g.,
			waiting until image acquisition is complete before rendering starts.

			- CPU-GPU sync tools—used to wait for GPU work to finish (like finishing a frame before starting the next).

	[] 15. Submit the command buffers to the graphics queue for execution.

	[] 16. Present the rendered images to the surface.

	[] 17. Cleanup resources when done.
*/


class Graphics
{
	// instance of cGraphics, used as a singleton (single instance throughout the application)
	// returns the instance of Graphics, creates it if it doesn't exist
public:
	void Initialize();
	~Graphics();

	// delete move/copy
	Graphics(const Graphics&) = delete;
	Graphics operator=(const Graphics& r) = delete;
private:
	Graphics();
	static std::shared_ptr<Graphics> Get();

	bool IsDebugSupported();
	void InitializeDebugger();
	void CleanupDebugger();
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	void InitializeWindow();
	void CreateWindowSurface();
	void CreateVulkanInstance();
	void CleanupVulkan();
	void GetVulkanData();
	bool IsLayerPresent(const std::string& layerName);
	bool IsExtensionPresent(const std::string& extensionName);
	bool IsDeviceSuitable(const VkPhysicalDevice& device);
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFrameBuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();

	friend class Lordi;

	static std::shared_ptr<Graphics> s_pInstance;

	const int cMaxFramesInFlight = 2; // max number of frames in flight, used for synchronization

	const std::vector<const char*> cRequiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> cRequiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	
	GLFWwindow* m_pWindow = nullptr;

	VkDebugUtilsMessengerEXT m_pDebugMessenger = nullptr;
	VkDebugUtilsMessengerCreateInfoEXT m_DebugCreateInfo = {};

	// Vulkan instance, the connection between the application and the Vulkan library
	VkInstance m_pInstance = nullptr;
	// Extensions are used to extend the functionality of Vulkan, each extension provides additional features or capabilities
	// These are the ones available on the system
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pGlobalExtensions = nullptr;
	// Validation layers (debugging functionality)
	std::unique_ptr<std::vector<VkLayerProperties>> m_pGlobalLayers = nullptr;

	// Physical device (GPU)
	VkPhysicalDevice m_pPhysicalDevice = nullptr; 
	// name, type, supported Vulkan version of device
	VkPhysicalDeviceProperties m_DeviceProperties = {};
	// optional features like texture compression, 64 bit floats and multi viewport rendering of device
	VkPhysicalDeviceFeatures m_DeviceFeatures = {};
	// extensions from device (essential features)
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pDeviceExtensions = nullptr;

	// Queue families of the GPU, each allows a subset of commands.
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

	// min/max image count, min/max image dimensions
	VkSurfaceCapabilitiesKHR m_SwapChainCapabilities = {};
	// Supported formats for the swap chain images, e.g. format (color channels and types) and color space (sRGB, linear, etc.)
	std::unique_ptr<std::vector<VkSurfaceFormatKHR>> m_pSwapChainFormats = nullptr;
	// Supported presentation modes for the swap chain, e.g. FIFO, mailbox, etc.
	// VK_PRESENT_MODE_IMMEDIATE_KHR - No VSync, images are presented immediately
	// VK_PRESENT_MODE_FIFO_KHR - VSync, images are presented in a queue, FIFO
	// VK_PRESENT_MODE_FIFO_RELAXED_KHR - VSync, images are presented in a queue, but can be presented earlier if the queue is empty
	// VK_PRESENT_MODE_MAILBOX_KHR - VSync (tripple buffering), images are presented in a queue, but can be replaced by a new image 
	std::unique_ptr<std::vector<VkPresentModeKHR>> m_pSwapChainPresentModes = nullptr;
	// Swap chain creation info
	VkSwapchainCreateInfoKHR m_SwapChainCreateInfo = {};
	// Swap chain
	VkSwapchainKHR m_pSwapChain = nullptr;
	// Swap chain image handles
	std::unique_ptr<std::vector<VkImage>> m_pSwapChainImages = nullptr;
	// Swap chain image views, used to access the swap chain images
	std::unique_ptr<std::vector<VkImageView>> m_pSwapChainImageViews = nullptr;

	// Vertex shader module
	VkShaderModule m_pVertexShaderModule = nullptr;
	// Fragment shader module
	VkShaderModule m_pFragmentShaderModule = nullptr;

	VkRenderPass m_pRenderPass = nullptr;
	// Pipeline layout, describes the layout of the pipeline, e.g. descriptor sets, push constants, etc.
	VkPipelineLayout m_pPipelineLayout = nullptr;
	VkPipeline m_pGraphicsPipeline = nullptr;

	// Framebuffers for the swap chain images, used to render to the swap chain images
	std::unique_ptr<std::vector<VkFramebuffer>> m_pFrameBuffers = nullptr;

	// Command pool for managing command buffers
	// Command buffers are executed by submitting them to one of the device's queues
	VkCommandPool m_pCommandPool = nullptr;

	std::unique_ptr<std::vector<VkCommandBuffer>> m_pCommandBuffers = nullptr;

	// Window surface, the area where the swap chain images will be presented
	VkSurfaceKHR m_pWindowSurface = nullptr;

	// Sync objects
	std::unique_ptr<std::vector<VkSemaphore>> m_pImageAvailableSemaphores = nullptr;
	std::unique_ptr<std::vector<VkSemaphore>> m_pRenderFinishedSemaphores = nullptr;
	std::unique_ptr<std::vector<VkFence>> m_pInFlightFences = nullptr;
	uint32_t m_uiCurrentFrame = 0;
};