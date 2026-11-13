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
		// Only for this frame, while the object manager holds it: no reference counting per draw.
		const Object* object = nullptr;
		uint32_t modelIndex = 0;
		uint32_t slot = 0;		// its place in the instance buffer
		float distance = 0.f;	// squared, from the camera
		glm::vec3 center = {};	// bounding sphere, world space
		float radius = 0.f;
		glm::mat4 model = glm::mat4(1.f);	// pushed straight to the shadow pass
	};
	// The sides of a view volume, for throwing out what can't be in it. Only the side and near
	// planes: what's past the far plane of a shadow view can't be there anyway.
	struct Frustum
	{
		std::array<glm::vec4, 5> planes = {};
		explicit Frustum(const glm::mat4& viewProj);
		bool Contains(const glm::vec3& center, float radius) const;
	};

	// A light-space matrix covering the view out to farDistance, snapped to whole shadow texels so
	// the shadows don't crawl as the camera moves.
	glm::mat4 CascadeMatrix(const glm::vec3& cameraPosition, const glm::vec3& forward, float tanHalfFov, float aspect,
		float farDistance, uint32_t resolution, float& texelWorldSize) const;
	bool ShadowsActive() const;
	void WriteSceneUniforms(const Camera& camera, float aspect, SceneUniforms& scene) const;
	void DrawList(vk::CommandBuffer& commandBuffer, const std::vector<const DrawItem*>& items, const vk::Pipeline& pipeline);
	// Draws the items, which come grouped by model, as one instanced draw per model, listing their
	// slots in this frame's draw list. The pipeline and descriptor sets are already bound.
	void DrawInstanced(vk::CommandBuffer& commandBuffer, const std::vector<const DrawItem*>& items);
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

	// Lamp shadow maps are only redrawn when what each lamp sees has changed (see the shadow pass).
	std::array<uint64_t, MAX_SHADOWED_LIGHTS>	m_lampShadowHashes = {};
	vk::Image									m_lampShadowCacheImage = {};
	// Lamps something has moved in reach of, waiting for their map to be redrawn: a few a frame, taking turns, so
	// something moving under several lamps (an engine rolled on its stand) doesn't redraw all their maps every frame.
	std::array<bool, MAX_SHADOWED_LIGHTS>		m_lampShadowStale = {};
	int											m_lampShadowTurn = 0;
	// The sun's cascades are kept the same way while nothing in them (nor the view) changes: standing
	// still at the bench, they aren't redrawn every frame.
	std::array<uint64_t, SHADOW_CASCADES>		m_cascadeShadowHashes = {};
	vk::Image									m_cascadeShadowCacheImage = {};
	// Which objects cast shadows last frame (a hash of them), to tell when one comes or goes.
	uint64_t									m_lastShadowCasters = 0;
	// How much of this frame's draw list the passes have used so far.
	uint32_t									m_drawCursor = 0;
};
