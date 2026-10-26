#include "DLSS.h"

#include <filesystem>
#include <format>

#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

#include "../Utility/Logger.h"

namespace
{
	// Identifies the game to NGX. A custom engine picks its own: any GUID that stays the same.
	constexpr const char* PROJECT_ID = "6f2b9d4e-1c7a-4e53-9a8d-3b5f0c2e7a19";
	constexpr const char* ENGINE_VERSION = "1.0";

	// The paths NGX is handed have to outlive every call, so they're built once.
	struct NGXPaths
	{
		std::wstring dataPath;
		std::vector<std::wstring> dllFolders;
		std::vector<const wchar_t*> dllFolderPointers;
		NVSDK_NGX_FeatureCommonInfo commonInfo = {};
	};

	NGXPaths& Paths()
	{
		static NGXPaths paths = [] {
			NGXPaths built;
			// Logs and NGX's own files, somewhere writable that isn't the project.
			std::error_code error;
			auto dataFolder = std::filesystem::temp_directory_path(error) / "Lordi";
			std::filesystem::create_directories(dataFolder, error);
			built.dataPath = dataFolder.wstring();

			// nvngx_dlss.dll is copied next to the executable when it's built; running from the
			// project folder it can also come straight from the SDK.
			built.dllFolders.push_back((std::filesystem::current_path(error) / "Libraries" / "DLSS" / "lib" / "Windows_x86_64" / "rel").wstring());
			for (const auto& folder : built.dllFolders)
				built.dllFolderPointers.push_back(folder.c_str());
			built.commonInfo.PathListInfo.Path = built.dllFolderPointers.data();
			built.commonInfo.PathListInfo.Length = static_cast<unsigned int>(built.dllFolderPointers.size());
			return built;
		}();
		return paths;
	}

	NVSDK_NGX_FeatureDiscoveryInfo DiscoveryInfo()
	{
		auto& paths = Paths();
		NVSDK_NGX_FeatureDiscoveryInfo info = {};
		info.SDKVersion = NVSDK_NGX_Version_API;
		info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
		info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
		info.Identifier.v.ProjectDesc = { PROJECT_ID, NVSDK_NGX_ENGINE_TYPE_CUSTOM, ENGINE_VERSION };
		info.ApplicationDataPath = paths.dataPath.c_str();
		info.FeatureInfo = &paths.commonInfo;
		return info;
	}

	NVSDK_NGX_PerfQuality_Value Quality(DLSSMode mode)
	{
		switch (mode)
		{
			case DLSSMode::DLAA:				return NVSDK_NGX_PerfQuality_Value_DLAA;
			case DLSSMode::QUALITY:				return NVSDK_NGX_PerfQuality_Value_MaxQuality;
			case DLSSMode::BALANCED:			return NVSDK_NGX_PerfQuality_Value_Balanced;
			case DLSSMode::PERFORMANCE:			return NVSDK_NGX_PerfQuality_Value_MaxPerf;
			case DLSSMode::ULTRA_PERFORMANCE:	return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
			default:							return NVSDK_NGX_PerfQuality_Value_MaxQuality;
		}
	}

	std::string ResultText(NVSDK_NGX_Result result)
	{
		return std::format("0x{:08X}", static_cast<uint32_t>(result));
	}

	NVSDK_NGX_Resource_VK Resource(const DLSS::Image& image, bool readWrite)
	{
		VkImageSubresourceRange range = { image.aspect, 0, 1, 0, 1 };
		return NVSDK_NGX_Create_ImageView_Resource_VK(image.view, image.image, range, image.format, image.width, image.height, readWrite);
	}
}

std::vector<std::string> DLSS::InstanceExtensions()
{
	auto info = DiscoveryInfo();
	uint32_t count = 0;
	VkExtensionProperties* extensions = nullptr;
	if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &extensions)) || !extensions)
		return {};

	std::vector<std::string> names;
	for (uint32_t i = 0; i < count; i++)
		names.push_back(extensions[i].extensionName);
	return names;
}

std::vector<std::string> DLSS::DeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice)
{
	auto info = DiscoveryInfo();
	uint32_t count = 0;
	VkExtensionProperties* extensions = nullptr;
	if (NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physicalDevice, &info, &count, &extensions)) || !extensions)
		return {};

	std::vector<std::string> names;
	for (uint32_t i = 0; i < count; i++)
		names.push_back(extensions[i].extensionName);
	return names;
}

bool DLSS::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
	LOG_INFO("Initializing NVIDIA NGX for DLSS...");
	m_device = device;
	auto& paths = Paths();

	NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(PROJECT_ID, NVSDK_NGX_ENGINE_TYPE_CUSTOM, ENGINE_VERSION,
		paths.dataPath.c_str(), instance, physicalDevice, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr, &paths.commonInfo);
	if (NVSDK_NGX_FAILED(result))
	{
		m_status = result == NVSDK_NGX_Result_FAIL_FeatureNotSupported || result == NVSDK_NGX_Result_FAIL_PlatformError
			? "DLSS needs an NVIDIA RTX graphics card."
			: "DLSS could not start (NGX error " + ResultText(result) + ").";
		LOG_WARNING(m_status);
		return false;
	}
	m_initialized = true;

	result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_parameters);
	if (NVSDK_NGX_FAILED(result) || !m_parameters)
	{
		m_status = "DLSS could not read the driver's capabilities (NGX error " + ResultText(result) + ").";
		LOG_WARNING(m_status);
		return false;
	}

	int needsNewerDriver = 0;
	unsigned int driverMajor = 0, driverMinor = 0;
	if (NVSDK_NGX_SUCCEED(NVSDK_NGX_Parameter_GetI(m_parameters, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsNewerDriver))
		&& needsNewerDriver)
	{
		NVSDK_NGX_Parameter_GetUI(m_parameters, NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &driverMajor);
		NVSDK_NGX_Parameter_GetUI(m_parameters, NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &driverMinor);
		m_status = std::format("DLSS needs NVIDIA driver {}.{} or newer.", driverMajor, driverMinor);
		LOG_WARNING(m_status);
		return false;
	}

	int available = 0;
	result = NVSDK_NGX_Parameter_GetI(m_parameters, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
	if (NVSDK_NGX_FAILED(result) || !available)
	{
		m_status = "DLSS isn't supported on this graphics card.";
		LOG_WARNING(m_status);
		return false;
	}

	m_available = true;
	m_status = "DLSS is available.";
	LOG_SUCCESS("DLSS is available!");
	return true;
}

void DLSS::Shutdown()
{
	ReleaseFeature();
	if (m_parameters)
		NVSDK_NGX_VULKAN_DestroyParameters(m_parameters);
	m_parameters = nullptr;
	if (m_initialized)
		NVSDK_NGX_VULKAN_Shutdown1(m_device);
	m_initialized = false;
	m_available = false;
}

bool DLSS::IsAvailable() const
{
	return m_available;
}

const std::string& DLSS::GetStatus() const
{
	return m_status;
}

bool DLSS::GetRenderSize(DLSSMode mode, uint32_t outputWidth, uint32_t outputHeight, uint32_t& renderWidth, uint32_t& renderHeight)
{
	if (!m_available || mode == DLSSMode::OFF)
		return false;

	void* callback = nullptr;
	NVSDK_NGX_Parameter_GetVoidPointer(m_parameters, NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback, &callback);
	if (!callback)
		return false;

	NVSDK_NGX_Parameter_SetUI(m_parameters, NVSDK_NGX_Parameter_Width, outputWidth);
	NVSDK_NGX_Parameter_SetUI(m_parameters, NVSDK_NGX_Parameter_Height, outputHeight);
	NVSDK_NGX_Parameter_SetI(m_parameters, NVSDK_NGX_Parameter_PerfQualityValue, Quality(mode));
	NVSDK_NGX_Parameter_SetI(m_parameters, NVSDK_NGX_Parameter_RTXValue, 0);
	if (NVSDK_NGX_FAILED(reinterpret_cast<PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback>(callback)(m_parameters)))
		return false;

	unsigned int width = 0, height = 0;
	NVSDK_NGX_Parameter_GetUI(m_parameters, NVSDK_NGX_Parameter_OutWidth, &width);
	NVSDK_NGX_Parameter_GetUI(m_parameters, NVSDK_NGX_Parameter_OutHeight, &height);
	if (!width || !height)
		return false;

	renderWidth = width;
	renderHeight = height;
	return true;
}

bool DLSS::CreateFeature(VkCommandBuffer commandBuffer, DLSSMode mode, uint32_t renderWidth, uint32_t renderHeight,
	uint32_t outputWidth, uint32_t outputHeight)
{
	ReleaseFeature();
	if (!m_available || mode == DLSSMode::OFF)
		return false;

	// The scene is tone mapped before DLSS sees it (LDR), its motion vectors are at render size and
	// don't include the jitter, and depth runs 0 near to 1 far.
	NVSDK_NGX_DLSS_Create_Params createParams = {};
	createParams.Feature.InWidth = renderWidth;
	createParams.Feature.InHeight = renderHeight;
	createParams.Feature.InTargetWidth = outputWidth;
	createParams.Feature.InTargetHeight = outputHeight;
	createParams.Feature.InPerfQualityValue = Quality(mode);
	createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;

	NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT1(m_device, commandBuffer, 1, 1, &m_feature, m_parameters, &createParams);
	if (NVSDK_NGX_FAILED(result))
	{
		m_feature = nullptr;
		m_status = "DLSS failed to start at this resolution (NGX error " + ResultText(result) + ").";
		LOG_WARNING(m_status);
		return false;
	}
	return true;
}

void DLSS::ReleaseFeature()
{
	if (m_feature)
		NVSDK_NGX_VULKAN_ReleaseFeature(m_feature);
	m_feature = nullptr;
}

bool DLSS::HasFeature() const
{
	return m_feature != nullptr;
}

bool DLSS::Evaluate(VkCommandBuffer commandBuffer, const Inputs& inputs)
{
	if (!m_feature)
		return false;

	NVSDK_NGX_Resource_VK color = Resource(inputs.color, false);
	NVSDK_NGX_Resource_VK depth = Resource(inputs.depth, false);
	NVSDK_NGX_Resource_VK motionVectors = Resource(inputs.motionVectors, false);
	NVSDK_NGX_Resource_VK output = Resource(inputs.output, true);

	NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {};
	evalParams.Feature.pInColor = &color;
	evalParams.Feature.pInOutput = &output;
	evalParams.pInDepth = &depth;
	evalParams.pInMotionVectors = &motionVectors;
	evalParams.InJitterOffsetX = inputs.jitterX;
	evalParams.InJitterOffsetY = inputs.jitterY;
	evalParams.InRenderSubrectDimensions = { inputs.color.width, inputs.color.height };
	evalParams.InReset = inputs.reset ? 1 : 0;
	evalParams.InMVScaleX = inputs.motionScaleX;
	evalParams.InMVScaleY = inputs.motionScaleY;
	evalParams.InFrameTimeDeltaInMsec = inputs.frameTimeMs;

	NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(commandBuffer, m_feature, m_parameters, &evalParams);
	if (NVSDK_NGX_FAILED(result))
	{
		LOG_WARNING("DLSS evaluate failed (NGX error " + ResultText(result) + ").");
		return false;
	}
	return true;
}
