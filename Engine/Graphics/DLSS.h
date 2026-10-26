#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "../Utility/Settings.h"

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

// NVIDIA DLSS Super Resolution through the NGX SDK. It only knows about NGX: the render targets,
// barriers and jitter are the renderer's (GraphicsUpscaling.cpp, Renderer.cpp).
class DLSS
{
public:
	struct Image
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct Inputs
	{
		Image color;			// render resolution, shader-read layout
		Image depth;			// render resolution, shader-read layout
		Image motionVectors;	// render resolution, shader-read layout
		Image output;			// display resolution, general layout
		float jitterX = 0.f;	// in render pixels
		float jitterY = 0.f;
		float motionScaleX = 1.f;
		float motionScaleY = 1.f;
		float frameTimeMs = 0.f;
		bool reset = false;
	};

	// Extensions NGX needs. Instance ones are asked for before there is an instance, device ones
	// before the device: enable whichever the driver has.
	static std::vector<std::string> InstanceExtensions();
	static std::vector<std::string> DeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice);

	// Starts NGX on the device. False (with a reason in GetStatus) when DLSS can't run here;
	// everything else is then a no-op.
	bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
	void Shutdown();

	bool IsAvailable() const;
	const std::string& GetStatus() const;

	// The resolution DLSS wants the scene rendered at for this mode and output size.
	bool GetRenderSize(DLSSMode mode, uint32_t outputWidth, uint32_t outputHeight, uint32_t& renderWidth, uint32_t& renderHeight);

	// The feature holds DLSS's history at one input/output size, so it is made again whenever
	// either changes.
	bool CreateFeature(VkCommandBuffer commandBuffer, DLSSMode mode, uint32_t renderWidth, uint32_t renderHeight,
		uint32_t outputWidth, uint32_t outputHeight);
	void ReleaseFeature();
	bool HasFeature() const;

	bool Evaluate(VkCommandBuffer commandBuffer, const Inputs& inputs);

private:
	bool m_initialized = false;
	bool m_available = false;
	std::string m_status = "DLSS has not started.";
	VkDevice m_device = VK_NULL_HANDLE;
	NVSDK_NGX_Parameter* m_parameters = nullptr;
	NVSDK_NGX_Handle* m_feature = nullptr;
};
