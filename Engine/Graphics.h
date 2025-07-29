#pragma once


#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

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
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
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
	bool IsDeviceSuitable(const vk::PhysicalDevice& device);
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFrameBuffers();
	void CreateCommandPools();
	void CreateCommandBuffers();
	void RecordCommandBuffer(const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
	void DrawFrame();
	void CreateSyncObjects();
	void RecreateSwapChain();
	void CleanupSwapChain();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	uint32_t FindMemoryTypeIndex(uint32_t memoryType, vk::MemoryPropertyFlags memoryProperties);
	void CreateBuffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags bufferUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags, vk::Buffer& buffer, vk::DeviceMemory& deviceMemory);
	void CopyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize);
	void CreateDescriptorSetLayout();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateDescriptorPool();
	void CreateDescriptorSets();

	friend class Lordi;

	// Singleton
	static std::shared_ptr<Graphics> s_pInstance;

	// Parameters
	const int m_ciMaxFramesInFlight = 2; // 2 - double buffering, 3 - tripple bufferingw
	std::vector<const char*> m_vRequiredValidationLayerNames = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> m_vRequiredDeviceExtensionNames = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// Window/surface
	bool m_bFramebufferResized = false;
	GLFWwindow* m_pWindow = nullptr;
	vk::SurfaceKHR m_pSurface = nullptr;

	// Vulkan instance
	vk::Instance m_pVulkanInstance = nullptr;
	std::vector<vk::ExtensionProperties> m_vVulkanExtensionProps = {};
	std::vector<vk::LayerProperties> m_vVulkanLayerProps = {};

	// Debug
	vk::DebugUtilsMessengerCreateInfoEXT m_sDebugCreateInfo = {};
	vk::DebugUtilsMessengerEXT m_pDebugMessenger = nullptr;

	// Physical device
	vk::PhysicalDevice m_pPhysicalDevice = nullptr; 
	std::vector<vk::QueueFamilyProperties> m_vPhysicalDeviceQueueFamiliesProps = {};
	uint32_t m_uiDeviceGraphicsIndex = 0;
	uint32_t m_uiDevicePresentIndex = 0;
	uint32_t m_uiDeviceTransferIndex = 0;
	vk::PhysicalDeviceFeatures m_sPhysicalDeviceFeatures = {};
	vk::PhysicalDeviceMemoryProperties m_sPhysicalDeviceMemoryProps = {};
	vk::PhysicalDeviceProperties m_sPhysicalDeviceProps = {};
	std::vector<vk::ExtensionProperties> m_vPhysicalDeviceExtensions = {};

	// Logical device
	vk::Device m_pLogicalDevice = nullptr;

	// Device queues
	vk::Queue m_pGraphicsQueue = nullptr;
	vk::Queue m_pPresentQueue = nullptr;
	vk::Queue m_pTransferQueue = nullptr;

	// Swapchain
	vk::SwapchainKHR m_pSwapChain = nullptr;
	vk::SwapchainCreateInfoKHR m_sSwapChainInfo = {};
	std::vector<vk::Image> m_vSwapChainImages = {};
	std::vector<vk::ImageView> m_vSwapChainImageViews = {};
	vk::SurfaceCapabilitiesKHR m_sSwapChainCapabilities = {};
	std::vector<vk::SurfaceFormatKHR> m_vSwapChainFormats = {};
	std::vector<vk::PresentModeKHR> m_vSwapChainPresentModes = {};
	vk::PresentModeKHR m_eUserPresentMode = vk::PresentModeKHR::eFifo;

	// Shaders
	vk::ShaderModule m_pVertexShaderModule = nullptr;
	vk::ShaderModule m_pFragmentShaderModule = nullptr;

	// Render pass
	vk::RenderPass m_pRenderPass = nullptr;
	vk::SampleCountFlagBits m_eMaxMSAA = vk::SampleCountFlagBits::e1;
	vk::SampleCountFlagBits m_eUserMSAA = vk::SampleCountFlagBits::e1;
	vk::DescriptorSetLayout m_pDescriptorSetLayout = nullptr;

	// Pipeline
	vk::Pipeline m_pGraphicsPipeline = nullptr;
	vk::PipelineLayout m_pPipelineLayout = nullptr;

	// Command pools
	vk::CommandPool m_pDrawingCommandPool = nullptr;
	vk::CommandPool m_pTransferCommandPool = nullptr;

	// Sync objects
	std::vector<vk::Semaphore> m_vImageAvailableSemaphores = {};
	std::vector<vk::Semaphore> m_vRenderFinishedSemaphores = {};
	std::vector<vk::Fence> m_vFrameInFlightFences = {};
	uint32_t m_uiCurrentFrame = 0;

	// Frame buffers
	std::vector<vk::Framebuffer> m_vFrameBuffers = {};

	// Vertex buffer
	vk::Buffer m_pVertexBuffer = nullptr;
	vk::DeviceMemory m_pVertexBufferMemory = nullptr;

	// Index buffer
	vk::Buffer m_pIndexBuffer = nullptr;
	vk::DeviceMemory m_pIndexBufferMemory = nullptr;

	// Command buffers
	std::vector<vk::CommandBuffer> m_vCommandBuffers = {};

	// Uniform buffers
	std::vector<vk::Buffer> m_vUniformBuffers = {};
	std::vector<vk::DeviceMemory> m_vUniformBuffersMemory = {};
	std::vector<void*> m_vUniformBuffersMapped = {};

	// Descriptor sets
	vk::DescriptorPool m_pDescriptorPool = nullptr;
	std::vector<vk::DescriptorSet> m_vDescriptorSets = {};
};