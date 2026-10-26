#pragma once

#include "../Objects/ObjectManager.h"
#include "Models/TextureManager.h"
#include "Models/ModelManager.h"
#include "../Objects/CameraManager.h"
#include "IMGUI/IMGUIManager.h"
#include "../Utility/PerformanceManager.h"
#include "Lighting.h"

class Renderer
{
	CLASS_SINGLETON_HEADER(Renderer)
public:
	vk::CommandBuffer RecordCommandBuffer(const uint32_t& imageIndex);
	void RenderFrame();
	Renderer();
	~Renderer();

	// Sun, sky, lamps and fog for the frames from now on.
	void SetSceneLighting(const SceneLighting& lighting);
	const SceneLighting& GetSceneLighting() const;

	// Changing the resolution rebuilds the shadow map, so only call it when the setting changes.
	void SetShadowSettings(const ShadowSettings& settings);
	const ShadowSettings& GetShadowSettings() const;

	// After a cut (loading a save, a teleport): DLSS drops what it remembers of earlier frames
	// instead of smearing them into the next one.
	void ResetTemporalHistory();
private:
	struct PreviousModel
	{
		glm::mat4 matrix = glm::mat4(1.f);
		uint64_t frame = 0;		// the frame it was drawn with this matrix
	};
	struct DrawItem
	{
		pObject object = nullptr;
		uint32_t modelIndex = 0;
		uint32_t dynamicOffset = 0;
		float distance = 0.f;	// squared, from the camera
	};

	// A light-space matrix covering the view out to farDistance, snapped to whole shadow texels so
	// the shadows don't crawl as the camera moves.
	glm::mat4 CascadeMatrix(const glm::vec3& cameraPosition, const glm::vec3& forward, float tanHalfFov, float aspect,
		float farDistance, uint32_t resolution, float& texelWorldSize) const;
	bool ShadowsActive() const;
	void WriteSceneUniforms(const Camera& camera, float aspect, SceneUniforms& scene) const;
	void DrawList(vk::CommandBuffer& commandBuffer, const std::vector<DrawItem>& items, const vk::Pipeline& pipeline) const;
	// This frame's sub-pixel camera offset for DLSS, in render pixels.
	glm::vec2 NextJitter(const vk::Extent2D& renderExtent, const vk::Extent2D& outputExtent);
	// DLSS upscales the scene targets into its output, which then goes on the swap chain.
	void RecordUpscale(vk::CommandBuffer& commandBuffer, const glm::vec2& jitter, bool reset);

	std::shared_ptr<Graphics>					m_graphics = nullptr;
	std::shared_ptr<ModelManager>				m_modelManger = nullptr;
	std::shared_ptr<TextureManager>				m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>				m_objectManager = nullptr;
	std::shared_ptr<CameraManager>				m_cameraManager = nullptr;
	std::shared_ptr<IMGUIManager>				m_imguiManager = nullptr;
	std::shared_ptr<PerformanceManager>			m_performanceManager = nullptr;

	SceneLighting								m_lighting = {};
	ShadowSettings								m_shadowSettings = { 0, 40.f };

	// Motion vectors: where everything was last frame
	std::vector<PreviousModel>					m_previousModels = {};
	glm::mat4									m_previousViewProj = glm::mat4(1.f);
	uint64_t									m_frameNumber = 0;
	uint32_t									m_jitterIndex = 0;
	bool										m_resetHistory = true;
	std::chrono::steady_clock::time_point		m_lastFrameTime = {};
	float										m_frameTimeMs = 0.f;
};
