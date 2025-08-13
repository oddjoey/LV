#pragma once

#include <stb_image.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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
#include <chrono>		// std::chrono

#include "../Utility.h"
#include "../Logger.h"
#include "../Settings.h"
#include "Models/Model.h"

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

struct UniformBuffer
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class Graphics
{
	// Singleton
	static std::shared_ptr<Graphics> s_instance;
public:
	friend class Renderer;
	friend class IMGUIManager;
	static std::shared_ptr<Graphics> Get();

	void CopyDataToBuffer(void* data, vk::DeviceSize size, vk::Buffer& buffer);
	vk::Buffer CreateBuffer(vk::DeviceSize bufferSize, vk::BufferUsageFlags bufferUsageFlags, vk::MemoryPropertyFlags memoryPropertyFlags,
		vk::DeviceMemory& deviceMemory, bool cleanup = false);
	void CopyBufferToBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize bufferSize);

	vk::Image CreateImage(uint32_t imageWidth, uint32_t imageHeight, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount,
		vk::Format imageFormat, vk::ImageTiling imageTiling, vk::ImageUsageFlags imageUsage, vk::MemoryPropertyFlags memoryProperties,
		vk::DeviceMemory& deviceMemory, bool cleanup = false);
	vk::ImageView CreateImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels, bool cleanup = false);
	void TransitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		uint32_t mipLevels, const vk::CommandBuffer* inCommandBuffer = nullptr);
	void CopyDataToImage(void* data, vk::DeviceSize size, vk::Image& image, uint32_t imageWidth, uint32_t imageHeight);
	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t imageWidth, uint32_t imageHeight);
	vk::Sampler CreateSampler(bool cleanup = false);
	void GenerateMipmaps(const vk::Image& image, const int32_t& width, const int32_t& height, const uint32_t& mipLevels);
	std::vector<vk::CommandBuffer> CreateCommandBuffers(const uint32_t& count);
	void BeginCommandBuffer(vk::CommandBuffer& commandBuffer, vk::CommandBufferUsageFlags usageFlags);

	~Graphics();

	// delete move/copy
	Graphics(const Graphics&) = delete;
	Graphics operator=(const Graphics& r) = delete;
private:
	Graphics();
	// Create GLFW window
	void InitializeWindow();

	// Create a Vulkan instance, specifying Vulkan API version, which extensions and layers we want to use, debug settings
	void GetVulkanData();
	void CreateVulkanInstance();
	void CleanupVulkan();

	// Check if cRequiredValidationLayers layers are present in Vulkan's instance layers for debugging purposes
	bool IsDebugSupported();
	// Start Vulkan debugger
	void InitializeDebugger();
	// Cleanup Vulkan debugger
	void CleanupDebugger();
	// Debug messages sent here to be printed to console
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
	static void GLFWErrorCallback(int error, const char* description);

	// Create a Vulkan surface from our GLFW window
	void CreateWindowSurface();
	// Window/framebuffer resizes are sent here
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	// Physical device
	void PickPhysicalDevice();
	bool IsDeviceSuitable(const vk::PhysicalDevice& device);
	void CreateLogicalDevice();

	// Swap chain
	void CreateSwapChain();
	void RecreateSwapChain();
	void CreateImageViews();
	void CleanupSwapChain();

	// Render pass
	void CreateRenderPass();

	// Pipeline
	void CreateGraphicsPipeline();

	// Frame buffers
	void CreateFrameBuffers();

	// Command buffers
	void CreateCommandPools();
	void CreateMainCommandBuffers();
	vk::CommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(vk::CommandBuffer commandBuffer);

	// Buffer helper functions
	uint32_t FindMemoryTypeIndex(uint32_t memoryType, vk::MemoryPropertyFlags memoryProperties);

	// Uniform buffers
	void CreateDynamicUniformBuffers();
	void UpdateUniformBuffer(uint32_t currentImage);

	// Descriptors
	void CreateDescriptorSetLayout(const uint32_t& numOfTextures);
	void CreateDescriptorPool(const uint32_t& numOfTextures);
	void CreateDescriptorSets(const std::vector<vk::DescriptorImageInfo>& textureDescriptors);

	// Sync
	void CreateSyncObjects();

	// Image helper functions
	bool HasStencilComponent(vk::Format format);

	// Depth
	void CreateDepthResources();

	// MSAA
	void CreateMSAAResources();

	friend class Lordi;
	friend class Renderer;
	friend class ModelManager;
	friend class InputManager;
	
	// Parameters
	std::vector<const char*> m_requiredValidationLayerNames = {
		"VK_LAYER_KHRONOS_validation"
	};
	std::vector<const char*> m_requiredDeviceExtensionNames = { 
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
	};

	// Window/surface
	bool m_framebufferResized = false;
	GLFWwindow* m_window = nullptr;
	vk::SurfaceKHR m_windowSurface = {};

	// Vulkan instance
	vk::ApplicationInfo applicationInfo = {};
	vk::Instance m_vulkanInstance = {};
	std::vector<vk::ExtensionProperties> m_vulkanExtensionProps = {};
	std::vector<vk::LayerProperties> m_vulkanLayerProps = {};

	// Debug
	vk::DebugUtilsMessengerCreateInfoEXT m_debugCreateInfo = {};
	vk::DebugUtilsMessengerEXT m_debugMessenger = {};

	// Physical device
	vk::PhysicalDevice m_physicalDevice = {};
	std::vector<vk::QueueFamilyProperties> m_physicalDeviceQueueFamiliesProps = {};
	uint32_t m_physicalDeviceGraphicsIndex = 0;
	uint32_t m_physicalDevicePresentIndex = 0;
	uint32_t m_physicalDeviceTransferIndex = 0;
	vk::PhysicalDeviceFeatures m_physicalDeviceFeatures = {};
	//vk::PhysicalDeviceFeatures2 m_physicalDeviceFeatures2 = {};
	//vk::PhysicalDeviceVulkan12Features m_physicalDeviceVK12Features = {};
	vk::PhysicalDeviceProperties m_physicalDeviceProps = {};
	vk::PhysicalDeviceMemoryProperties m_physicalDeviceMemoryProps = {};
	vk::FormatProperties m_physicalDeviceFormatProps = {};
	std::vector<vk::ExtensionProperties> m_physicalDeviceExtensions = {};

	// Logical device
	vk::Device m_logicalDevice = {};

	// Device queues
	vk::Queue m_queueGraphics = {};
	vk::Queue m_queuePresent = {};
	vk::Queue m_queueTransfer = {};

	// Swap chain
	vk::SwapchainKHR m_swapChain = {};
	vk::SwapchainCreateInfoKHR m_swapChainInfo = {};
	std::vector<vk::Image> m_swapChainImages = {};
	std::vector<vk::ImageView> m_swapChainImageViews = {};
	vk::SurfaceCapabilitiesKHR m_windowSurfaceCapabilities = {};
	std::vector<vk::SurfaceFormatKHR> m_swapChainFormats = {};
	vk::SurfaceFormatKHR m_swapChainFormat = {};
	std::vector<vk::PresentModeKHR> m_swapChainPresentModes = {};
	vk::PresentModeKHR m_swapChainRequestedPresentMode = vk::PresentModeKHR::eFifo;
	vk::PresentModeKHR m_swapChainPresentMode = vk::PresentModeKHR::eFifo;
	uint32_t m_swapChainImageCount = 0;

	// Shaders
	vk::ShaderModule m_shaderModuleVertex = {};
	vk::ShaderModule m_shaderModuleFragment = {};

	// Render pass
	vk::RenderPass m_renderPass = {};

	// Pipeline
	vk::PipelineLayout m_pipelineLayout = {};
	vk::Pipeline m_pipelineGraphics = {};
	vk::PipelineCache m_pipelineCache = {};

	// Command pools
	vk::CommandPool m_commandPoolDraw = {};
	vk::CommandPool m_commandPoolTransfer = {};

	// Sync objects
	const int m_maxFramesInFlight = 2;
	std::vector<vk::Semaphore> m_imageAvailableSemaphores = {};
	std::vector<vk::Semaphore> m_mainRenderFinishedSemaphores = {};
	std::vector<vk::Semaphore> m_imguiRenderFinishedSemaphores = {};
	std::vector<vk::Fence> m_frameInFlightFences = {};
	uint32_t m_frameInFlightIndex = 0;

	// Frame buffers
	std::vector<vk::Framebuffer> m_frameBuffers = {};

	// Command buffers
	std::vector<vk::CommandBuffer> m_commandBuffers = {};

	// Uniform buffers
	std::vector<vk::Buffer> m_dynamicUniformBuffers = {};
	std::vector<vk::DeviceMemory> m_dynamicUniformBuffersMemory = {};
	std::vector<void*> m_dynamicUniformBuffersMapped = {};
	vk::DeviceSize m_alignedUBOSize;

	// Descriptor sets
	vk::DescriptorPool m_graphicsDescriptorPool = {};
	vk::DescriptorSetLayout m_dynamicUBODescriptorSetLayout = {};
	vk::DescriptorSetLayout m_samplerDescriptorSetLayout = {};
	std::vector<vk::DescriptorSet> m_dynamicUBODescriptorSets = {};
	vk::DescriptorSet m_samplerDescriptorSet = {};

	// Depth
	vk::Image m_depthImage = {};
	vk::DeviceMemory m_depthImageMemory = {};
	vk::ImageView m_depthImageView = {};
	vk::Format m_depthImageFormat = {};

	// Multisampling
	vk::SampleCountFlagBits m_MSAAMax = vk::SampleCountFlagBits::e1;
	vk::SampleCountFlagBits m_MSAALevel = vk::SampleCountFlagBits::e1;
	vk::Image m_MSAAImage = {};
	vk::DeviceMemory m_MSAAImageMemory = {};
	vk::ImageView m_MSAAImageView = {};

	// Memory managment
	std::vector<vk::Buffer>					m_manageBuffers = {};
	std::vector<vk::Image>					m_manageImages = {};
	std::vector<vk::ImageView>				m_manageImageViews = {};
	std::vector<vk::Sampler>				m_manageSamplers = {};
	std::vector<vk::DeviceMemory>			m_manageMemories = {};
};