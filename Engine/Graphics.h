#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>		// std::cout
#include <vector>		// std::vector
#include <memory>		// std::shared_ptr, std::unique_ptr
#include <set>			// std::set
#include <algorithm>	// std::clamp
#include <array>		// std::array
#include <chrono>		// time

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

		[done!] 14. Create synchronization objects (semaphores and fences) to synchronize the rendering process.

			- GPU-GPU sync primitives used to control the order of operations—e.g.,
			waiting until image acquisition is complete before rendering starts.

			- CPU-GPU sync tools—used to wait for GPU work to finish (like finishing a frame before starting the next).

	[done!] 15. Submit the command buffers to the graphics queue for execution.

	[done!] 16. Present the rendered images to the surface.

	[done!] 17. Cleanup resources when done.
*/

struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0; // location in the shader
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // position is a vec2 (2 floats)
		attributeDescriptions[0].offset = offsetof(Vertex, position); // offset in the struct

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1; // location in the shader
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // color is a vec3 (3 floats)
		attributeDescriptions[1].offset = offsetof(Vertex, color); // offset in the struct
		return attributeDescriptions;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

class Graphics
{
public:
	void Initialize();
	~Graphics();

	// delete move/copy
	Graphics(const Graphics&) = delete;
	Graphics operator=(const Graphics& r) = delete;
private:
	Graphics();
	static std::shared_ptr<Graphics> Get();

	// Check if cRequiredValidationLayers layers are present in Vulkan's instance layers for debugging purposes
	bool IsDebugSupported();
	// Start Vulkan debugger
	void InitializeDebugger();
	// Cleanup Vulkan debugger
	void CleanupDebugger();
	// Debug messages sent here to be printed to console
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	// Create GLFW window
	void InitializeWindow();
	// Window/framebuffer resizes are sent here
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	// Create a Vulkan surface from our GLFW window
	void CreateWindowSurface();
	// Create a Vulkan instance, specifying Vulkan API version, which extensions and layers we want to use, debug settings
	void CreateVulkanInstance();
	void CleanupVulkan();
	void GetVulkanData();
	bool IsDeviceSuitable(const VkPhysicalDevice& device);
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFrameBuffers();
	void CreateCommandPools();
	void CreateCommandBuffers();
	void RecordCommandBuffer(const VkCommandBuffer& commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();
	void RecreateSwapChain();
	void CleanupSwapChain();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	uint32_t FindMemoryTypeIndex(uint32_t memoryType, VkMemoryPropertyFlags memoryProperties);
	void CreateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsageFlags,
		VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& deviceMemory);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize);
	void CreateDescriptorSetLayout();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	friend class Lordi;

	static std::shared_ptr<Graphics> s_pInstance;

	const std::vector<const char*> cRequiredValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> cRequiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	bool m_bFramebufferResized = false;
	GLFWwindow* m_pWindow = nullptr;
	VkSurfaceKHR m_pWindowSurface = nullptr;

	VkInstance m_pVulkanInstance = nullptr;
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pInstanceExtensions = nullptr;
	std::unique_ptr<std::vector<VkLayerProperties>> m_pInstanceLayers = nullptr;

	VkDebugUtilsMessengerCreateInfoEXT m_DebugCreateInfo = {};
	VkDebugUtilsMessengerEXT m_pDebugMessenger = nullptr;

	VkPhysicalDevice m_pPhysicalDevice = nullptr; 
	std::unique_ptr<std::vector<VkQueueFamilyProperties>> m_pPhysicalDeviceQueueFamiliesProperties = nullptr;
	VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures = {};
	VkPhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties = {};
	VkPhysicalDeviceProperties m_PhysicalDeviceProperties = {};
	std::unique_ptr<std::vector<VkExtensionProperties>> m_pPhysicalDeviceExtensions = nullptr;
	uint32_t m_uiDeviceGraphicsIndex = 0;
	uint32_t m_uiDevicePresentIndex = 0;
	uint32_t m_uiDeviceTransferIndex = 0;

	VkDevice m_pLogicalDevice = nullptr;
	VkQueue m_pGraphicsQueue = nullptr;
	VkQueue m_pPresentQueue = nullptr;
	VkQueue m_pTransferQueue = nullptr;

	VkSwapchainKHR m_pSwapChain = nullptr;
	std::unique_ptr<std::vector<VkImage>> m_pSwapChainImages = nullptr;
	std::unique_ptr<std::vector<VkImageView>> m_pSwapChainImageViews = nullptr;
	VkSurfaceCapabilitiesKHR m_SwapChainCapabilities = {};
	std::unique_ptr<std::vector<VkSurfaceFormatKHR>> m_pSwapChainFormats = nullptr;
	std::unique_ptr<std::vector<VkPresentModeKHR>> m_pSwapChainPresentModes = nullptr;
	VkSwapchainCreateInfoKHR m_SwapChainCreateInfo = {};

	VkShaderModule m_pVertexShaderModule = nullptr;
	VkShaderModule m_pFragmentShaderModule = nullptr;
	VkSampleCountFlagBits m_eMaxMSAA = VK_SAMPLE_COUNT_1_BIT;
	VkSampleCountFlagBits m_eUserMSAA = VK_SAMPLE_COUNT_1_BIT;
	VkRenderPass m_pRenderPass = nullptr;
	VkDescriptorSetLayout m_pDescriptorSetLayout = nullptr;
	VkPipelineLayout m_pPipelineLayout = nullptr;
	VkPipeline m_pGraphicsPipeline = nullptr;
	std::unique_ptr<std::vector<VkFramebuffer>> m_pFrameBuffers = nullptr;
	VkCommandPool m_pDrawingCommandPool = nullptr;
	VkCommandPool m_pTransferCommandPool = nullptr;
	std::unique_ptr<std::vector<VkCommandBuffer>> m_pCommandBuffers = nullptr;
	
	std::unique_ptr<std::vector<VkSemaphore>> m_pImageAvailableSemaphores = nullptr;
	std::unique_ptr<std::vector<VkSemaphore>> m_pRenderFinishedSemaphores = nullptr;
	std::unique_ptr<std::vector<VkFence>> m_pFrameInFlightFences = nullptr;
	const int cMaxFramesInFlight = 2;
	uint32_t m_uiCurrentFrame = 0;

	VkBuffer m_pVertexBuffer = nullptr;
	VkDeviceMemory m_pVertexBufferMemory = nullptr;
	VkBuffer m_pIndexBuffer = nullptr;
	VkDeviceMemory m_pIndexBufferMemory = nullptr;

	std::unique_ptr<std::vector<VkBuffer>> m_pUniformBuffers;
	std::unique_ptr<std::vector<VkDeviceMemory>> m_pUniformBuffersMemory;
	std::unique_ptr<std::vector<void*>> m_pUniformBuffersMapped;

	VkDescriptorPool m_pDescriptorPool = nullptr;
	std::unique_ptr<std::vector<VkDescriptorSet>> m_pDescriptorSets = nullptr;
};