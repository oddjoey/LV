#include "Renderer.h"

CLASS_SINGLETON_SOURCE(Renderer)

Renderer::Frustum::Frustum(const glm::mat4& m)
{
	// Gribb-Hartmann: each plane is the last row of the matrix plus or minus another. The near plane
	// uses the -w <= z form, which also holds for 0-to-1 depth, just less tightly.
	glm::vec4 rowX = { m[0][0], m[1][0], m[2][0], m[3][0] };
	glm::vec4 rowY = { m[0][1], m[1][1], m[2][1], m[3][1] };
	glm::vec4 rowZ = { m[0][2], m[1][2], m[2][2], m[3][2] };
	glm::vec4 rowW = { m[0][3], m[1][3], m[2][3], m[3][3] };
	planes = { rowW + rowX, rowW - rowX, rowW + rowY, rowW - rowY, rowW + rowZ };
	for (auto& plane : planes)
		plane /= (std::max)(glm::length(glm::vec3(plane)), 1e-6f);
}

bool Renderer::Frustum::Contains(const glm::vec3& center, float radius) const
{
	for (const auto& plane : planes)
		if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
			return false;
	return true;
}

vk::CommandBuffer Renderer::RecordCommandBuffer(const uint32_t& imageIndex)
{
	auto commandBuffer = m_graphics->m_commandBuffers[m_graphics->m_frameInFlightIndex];
	commandBuffer.reset();

	vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = nullptr
	};
	if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		throw std::runtime_error("Failed begin on command buffer!");

	auto frame = m_graphics->m_frameInFlightIndex;
	auto camera = m_cameraManager->GetActiveCamera();
	auto extent = m_graphics->m_swapChainInfo.imageExtent;
	float aspect = static_cast<float>(m_graphics->m_windowSurfaceCapabilities.currentExtent.width) /
		static_cast<float>((std::max)(m_graphics->m_windowSurfaceCapabilities.currentExtent.height, 1u));

	// With DLSS the scene is drawn smaller, jittered by a different sub-pixel offset each frame.
	bool upscaling = camera && m_graphics->m_upscaling;
	vk::Extent2D renderExtent = upscaling ? m_graphics->m_renderExtent : extent;
	glm::vec2 jitter = upscaling ? NextJitter(renderExtent, extent) : glm::vec2(0.f);
	glm::vec2 jitterClip = jitter * 2.f / glm::vec2(static_cast<float>(renderExtent.width), static_cast<float>(renderExtent.height));
	bool resetHistory = m_resetHistory || m_graphics->m_upscaleHistoryLost;
	m_frameNumber++;

	// Everything drawn this frame gets its matrices up front: the shadow and main passes share them.
	// opaque holds every solid object (shadow casters can be out of view); the main pass draws only
	// what's in front of the camera.
	std::vector<DrawItem> opaque;
	// Points into opaque, which is reserved for every object up front and so never moves.
	std::vector<const DrawItem*> visibleOpaque;
	std::vector<DrawItem> transparent;
	std::vector<const DrawItem*> visibleTransparent;	// the same, furthest first
	std::vector<glm::vec4> movedCasters;	// shadow casters that moved since last frame: centre and radius
	opaque.reserve(MAX_OBJECTS);
	visibleOpaque.reserve(MAX_OBJECTS);
	// Every pass this frame lists the objects it draws in the draw list, from the start.
	m_drawCursor = 0;
	SceneUniforms scene = {};
	glm::mat4 view = glm::mat4(1.f), jitteredProjection = glm::mat4(1.f);
	if (camera)
	{
		glm::mat4 projection = glm::perspective(glm::radians(camera->GetFov()), aspect, camera->GetNearPlane(), camera->GetFarPlane());
		projection[1][1] *= -1;
		view = camera->CalculateViewMatrix();
		glm::mat4 viewProj = projection * view;
		if (resetHistory)
			m_previousViewProj = viewProj;

		jitteredProjection = glm::translate(glm::mat4(1.f), glm::vec3(jitterClip, 0.f)) * projection;
		auto instances = static_cast<InstanceData*>(m_graphics->m_instanceBuffersMapped[frame]);
		glm::vec3 cameraPosition = camera->GetPosition();
		if (m_previousModels.size() < MAX_OBJECTS)
			m_previousModels.resize(MAX_OBJECTS);
		Frustum cameraView(viewProj);

		for (auto& objectList : m_objectManager->GetObjectsByModel())
		{
			uint32_t modelIndex = objectList.first;
			if (!modelIndex)
				continue;
			auto model = m_modelManger->GetModel(modelIndex);
			glm::vec3 localCenter = model ? (model->GetBoundsMin() + model->GetBoundsMax()) * 0.5f : glm::vec3(0.f);
			float localRadius = model ? glm::length(model->GetBoundsMax() - model->GetBoundsMin()) * 0.5f : 1000.f;

			for (auto& object : objectList.second)
			{
				auto objectIndex = object->GetIndex();
				if (!object->IsVisible() || objectIndex == 0 || objectIndex > MAX_OBJECTS)
					continue;

				// Its slot in the instance buffer, filled here and copied in whole: the buffer is
				// write-combined memory, slow to read back.
				InstanceData instance;
				instance.model = object->GetModelMatrix();
				// Something that wasn't drawn last frame has no motion to report.
				auto& previous = m_previousModels[objectIndex - 1];
				bool moved = previous.frame + 1 != m_frameNumber || previous.matrix != instance.model;
				instance.previousModel = !resetHistory && previous.frame + 1 == m_frameNumber ? previous.matrix : instance.model;
				previous = { instance.model, m_frameNumber };
				instance.tint = object->GetTint();
				instance.emissive = object->GetEmissive();
				instance.textureIndex = object->GetTextureIndex();
				instance.shading = static_cast<uint32_t>(object->GetShading());
				instance.flags = object->IsInFront() ? INSTANCE_IN_FRONT : 0u;
				uint32_t slot = objectIndex - 1;
				memcpy(instances + slot, &instance, sizeof(instance));

				glm::vec3 scale = object->GetScale();
				float scaleMax = (std::max)({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
				glm::vec3 center = glm::vec3(instance.model * glm::vec4(localCenter, 1.f));
				glm::vec3 offset = object->GetPosition() - cameraPosition;
				DrawItem item = { object.get(), modelIndex, slot, glm::dot(offset, offset), center, localRadius * scaleMax, instance.model };
				bool inView = cameraView.Contains(item.center, item.radius);
				if (object->IsTransparent())
				{
					if (inView)
						transparent.push_back(item);
				}
				else
				{
					opaque.push_back(item);
					if (moved && object->CastsShadows() && object->CastsLampShadows())
						movedCasters.push_back(glm::vec4(item.center, item.radius));
					if (inView)
						visibleOpaque.push_back(&opaque.back());
				}
			}
		}
		std::sort(transparent.begin(), transparent.end(), [](const DrawItem& a, const DrawItem& b) { return a.distance > b.distance; });
		for (const auto& item : transparent)
			visibleTransparent.push_back(&item);

		WriteSceneUniforms(*camera, aspect, scene);
		scene.previousViewProj = m_previousViewProj;
		scene.view = view;
		scene.proj = jitteredProjection;
		// Textures sharp enough for the window's size, not the smaller render size (NVIDIA's advice).
		float lodBias = upscaling ? std::log2(static_cast<float>(renderExtent.width) / static_cast<float>(extent.width)) - 1.f : 0.f;
		scene.renderParams = glm::vec4(jitterClip, lodBias, 0.f);
		memcpy(m_graphics->m_sceneUniformBuffersMapped[frame], &scene, sizeof(scene));
		m_previousViewProj = viewProj;
	}

	vk::DeviceSize vertexOffset = 0;

	// One depth-only pass into a shadow map layer, drawing the solid lit objects that cast shadows
	// (and, for lamps, only those that can be within reach).
	// minRadius drops casters too small to show up in that map at all. With cachedHash, the map is
	// only redrawn when something in it has moved (or come or gone): it keeps its contents otherwise.
	// Everything that can cast a shadow at all, picked out once for every pass this frame.
	std::vector<const DrawItem*> shadowCasters;
	shadowCasters.reserve(opaque.size());
	for (const auto& item : opaque)
		if (item.object->CastsShadows() && item.object->GetShading() == ObjectShading::LIT)
			shadowCasters.push_back(&item);
	std::vector<const DrawItem*> casters;
	casters.reserve(opaque.size());
	auto shadowPass = [&](const vk::Framebuffer& framebuffer, uint32_t resolution, const glm::mat4& lightViewProj,
		const glm::vec3* lampPosition, float reach, float minRadius, uint64_t* cachedHash) {
		Frustum lightView(lightViewProj);
		casters.clear();
		for (const auto* item : shadowCasters)
		{
			if (item->radius < minRadius)
				continue;
			if (lampPosition)
			{
				if (!item->object->CastsLampShadows())
					continue;
				glm::vec3 away = item->center - *lampPosition;
				float limit = reach + item->radius;
				if (glm::dot(away, away) > limit * limit)
					continue;
			}
			// Out of the light's view: it can't cast into this map.
			if (!lightView.Contains(item->center, item->radius))
				continue;
			casters.push_back(item);
		}

		if (cachedHash)
		{
			// An FNV-style hash, a 64-bit word at a time, over what each caster is and where (its model
			// matrix holds position, rotation and scale).
			uint64_t hash = 1469598103934665603ull;
			auto mix = [&](const void* data, size_t size) {
				auto words = static_cast<const uint64_t*>(data);
				for (size_t i = 0; i < size / sizeof(uint64_t); i++)
				{
					hash = (hash ^ words[i]) * 1099511628211ull;
					hash ^= hash >> 29;
				}
			};
			mix(&lightViewProj, sizeof(lightViewProj));
			for (const auto* item : casters)
			{
				uint64_t identity[2] = { reinterpret_cast<uint64_t>(item->object), item->modelIndex };
				mix(identity, sizeof(identity));
				mix(&item->model, sizeof(item->model));
			}
			if (hash == *cachedHash)
				return;
			*cachedHash = hash;
		}

		vk::Viewport shadowViewport { 0.f, 0.f, static_cast<float>(resolution), static_cast<float>(resolution), 0.f, 1.f };
		vk::Rect2D shadowScissor { { 0, 0 }, { resolution, resolution } };
		vk::ClearValue clearShadow = { .depthStencil = { 1.f, 0 } };
		vk::RenderPassBeginInfo shadowBeginInfo {
			.renderPass = m_graphics->m_shadowRenderPass,
			.framebuffer = framebuffer,
			.renderArea = shadowScissor,
			.clearValueCount = 1,
			.pClearValues = &clearShadow
		};
		commandBuffer.beginRenderPass(&shadowBeginInfo, vk::SubpassContents::eInline);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics->m_shadowPipeline);
		commandBuffer.setViewport(0, 1, &shadowViewport);
		commandBuffer.setScissor(0, 1, &shadowScissor);
		commandBuffer.bindVertexBuffers(0, 1, &m_modelManger->m_vertexBuffer, &vertexOffset);
		commandBuffer.bindIndexBuffer(m_modelManger->m_indexBuffer, 0, vk::IndexType::eUint32);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics->m_shadowPipelineLayout, 0, 1,
			&m_graphics->m_instanceDescriptorSets[frame], 0, nullptr);
		commandBuffer.pushConstants(m_graphics->m_shadowPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &lightViewProj);
		DrawInstanced(commandBuffer, casters);
		commandBuffer.endRenderPass();
	};

	// Sun shadows: one pass per cascade.
	if (m_cascadeShadowCacheImage != m_graphics->m_shadowImage)
	{
		m_cascadeShadowHashes.fill(0);
		m_cascadeShadowCacheImage = m_graphics->m_shadowImage;
	}
	if (camera && scene.shadowParams.z > 0.5f)
		for (int cascade = 0; cascade < SHADOW_CASCADES; cascade++)
		{
			// In the coarse cascade, a part smaller than a couple of shadow texels casts nothing visible.
			float texel = cascade == 0 ? scene.shadowParams.x : scene.shadowParams.y;
			shadowPass(m_graphics->m_shadowFramebuffers[cascade], m_graphics->m_shadowResolution, scene.lightViewProj[cascade], nullptr, 0.f,
				cascade == 0 ? 0.f : texel, &m_cascadeShadowHashes[cascade]);
		}

	// Lamp shadows: one pass per shadowed light, by each caster's bounding sphere. Lamps don't move,
	// so a lamp's map is kept from frame to frame and only redrawn when what it sees changes. A new
	// shadow image (quality changed) starts the cache over.
	if (camera)
	{
		if (m_lampShadowCacheImage != m_graphics->m_lightShadowImage)
		{
			m_lampShadowHashes.fill(0);
			m_lampShadowCacheImage = m_graphics->m_lightShadowImage;
		}
		// A lamp's map only needs looking at again when a caster has come or gone, or one has moved within
		// its reach. Most frames, none has, and the per-lamp work below is skipped altogether.
		uint64_t casterSet = 1469598103934665603ull;
		for (const auto* item : shadowCasters)
			casterSet = (casterSet ^ reinterpret_cast<uint64_t>(item->object)) * 1099511628211ull;
		bool casterSetChanged = casterSet != m_lastShadowCasters;
		m_lastShadowCasters = casterSet;
		int count = (std::min)(static_cast<int>(m_lighting.pointLights.size()), MAX_POINT_LIGHTS);
		// Which point light each shadow layer belongs to this frame.
		std::array<int, MAX_SHADOWED_LIGHTS> layerLight;
		layerLight.fill(-1);
		for (int i = 0; i < count; i++)
		{
			int layer = static_cast<int>(scene.lightShadow[i].x);
			if (scene.lightShadow[i].x < 0.f || layer >= MAX_SHADOWED_LIGHTS)
				continue;
			layerLight[layer] = i;
			const auto& light = m_lighting.pointLights[i];
			// A new map, or a caster come or gone: redrawn now (that's rare).
			if (casterSetChanged || m_lampShadowHashes[layer] == 0)
			{
				m_lampShadowStale[layer] = false;
				shadowPass(m_graphics->m_lightShadowFramebuffers[layer], m_graphics->m_lightShadowResolution, scene.lightShadowViewProj[layer],
					&light.position, light.range, 0.f, &m_lampShadowHashes[layer]);
				continue;
			}
			for (const auto& sphere : movedCasters)
			{
				glm::vec3 away = glm::vec3(sphere) - light.position;
				float limit = light.range + sphere.w;
				if (glm::dot(away, away) <= limit * limit)
				{
					m_lampShadowStale[layer] = true;
					break;
				}
			}
		}
		// Something moving in reach: one of the waiting maps a frame, in turn.
		constexpr int MOVED_REDRAWS_PER_FRAME = 1;
		for (int tried = 0, redrawn = 0; tried < MAX_SHADOWED_LIGHTS && redrawn < MOVED_REDRAWS_PER_FRAME; tried++)
		{
			int layer = m_lampShadowTurn;
			m_lampShadowTurn = (m_lampShadowTurn + 1) % MAX_SHADOWED_LIGHTS;
			if (!m_lampShadowStale[layer] || layerLight[layer] < 0)
				continue;
			m_lampShadowStale[layer] = false;
			const auto& light = m_lighting.pointLights[layerLight[layer]];
			shadowPass(m_graphics->m_lightShadowFramebuffers[layer], m_graphics->m_lightShadowResolution, scene.lightShadowViewProj[layer],
				&light.position, light.range, 0.f, &m_lampShadowHashes[layer]);
			redrawn++;
		}
	}

	vk::ClearValue clearColor = {
		.color = std::array<float, 4>{ m_lighting.fogColor.r, m_lighting.fogColor.g, m_lighting.fogColor.b, 1.0f }
	};
	vk::ClearValue clearDepth = {
		.depthStencil = { 1.f, 0 }
	};

	if (upscaling)
	{
		// The scene into DLSS's targets at the render size: colour, motion vectors and depth.
		vk::ClearValue clearMotion = { .color = std::array<float, 4>{ 0.f, 0.f, 0.f, 0.f } };
		std::array<vk::ClearValue, 3> sceneClears = { clearColor, clearMotion, clearDepth };
		vk::Rect2D sceneArea = { .offset = { 0, 0 }, .extent = renderExtent };
		vk::RenderPassBeginInfo sceneBeginInfo = {
			.renderPass = m_graphics->m_upscaleRenderPass,
			.framebuffer = m_graphics->m_upscaleFramebuffer,
			.renderArea = sceneArea,
			.clearValueCount = static_cast<uint32_t>(sceneClears.size()),
			.pClearValues = sceneClears.data()
		};
		commandBuffer.beginRenderPass(&sceneBeginInfo, vk::SubpassContents::eInline);
		vk::Viewport sceneViewport { 0.f, 0.f, static_cast<float>(renderExtent.width), static_cast<float>(renderExtent.height), 0.f, 1.f };
		commandBuffer.setViewport(0, 1, &sceneViewport);
		commandBuffer.setScissor(0, 1, &sceneArea);
		commandBuffer.bindVertexBuffers(0, 1, &m_modelManger->m_vertexBuffer, &vertexOffset);
		commandBuffer.bindIndexBuffer(m_modelManger->m_indexBuffer, 0, vk::IndexType::eUint32);
		DrawList(commandBuffer, visibleOpaque, m_graphics->m_upscalePipeline);
		DrawList(commandBuffer, visibleTransparent, m_graphics->m_upscaleTransparentPipeline);
		commandBuffer.endRenderPass();

		RecordUpscale(commandBuffer, jitter, resetHistory);
	}
	m_resetHistory = false;
	m_graphics->m_upscaleHistoryLost = false;
	std::vector<vk::ClearValue> clearValues = { clearColor, clearDepth };
	vk::Rect2D renderArea = {
		.offset = {0, 0},
		.extent = m_graphics->m_swapChainInfo.imageExtent
	};
	vk::RenderPassBeginInfo rpBeginInfo = {
		.renderPass = m_graphics->m_renderPass,
		.framebuffer = m_graphics->m_frameBuffers[imageIndex],
		.renderArea = renderArea,
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};
	commandBuffer.beginRenderPass(&rpBeginInfo, vk::SubpassContents::eInline);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics->m_pipelineGraphics);
	vk::Viewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.width),
		.height = static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	commandBuffer.setViewport(0, 1, &viewport);
	vk::Rect2D scissor{
		.offset = { 0, 0 },
		.extent = m_graphics->m_swapChainInfo.imageExtent
	};
	commandBuffer.setScissor(0, 1, &scissor);

	if (!camera)
		return commandBuffer;

	if (upscaling)
	{
		// DLSS's output, already at the window's size; the UI goes over it in the next subpass.
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics->m_compositePipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics->m_compositePipelineLayout, 0, 1,
			&m_graphics->m_compositeDescriptorSet, 0, nullptr);
		commandBuffer.draw(3, 1, 0, 0);
		return commandBuffer;
	}

	commandBuffer.bindVertexBuffers(0, 1, &m_modelManger->m_vertexBuffer, &vertexOffset);
	commandBuffer.bindIndexBuffer(m_modelManger->m_indexBuffer, 0, vk::IndexType::eUint32);

	// Solid things first; glass blends over them, furthest first.
	DrawList(commandBuffer, visibleOpaque, m_graphics->m_pipelineGraphics);
	DrawList(commandBuffer, visibleTransparent, m_graphics->m_pipelineTransparent);

	return commandBuffer;
}

void Renderer::DrawList(vk::CommandBuffer& commandBuffer, const std::vector<const DrawItem*>& items, const vk::Pipeline& pipeline)
{
	if (items.empty())
		return;

	auto frame = m_graphics->m_frameInFlightIndex;
	std::array<vk::DescriptorSet, 3> descriptorSets = {
		m_graphics->m_instanceDescriptorSets[frame],
		m_graphics->m_samplerDescriptorSet,
		m_graphics->m_sceneDescriptorSets[frame]
	};

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	// Bound once for the whole list: each object's data is found through its slot in the draw list.
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics->m_pipelineLayout, 0,
		static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
	DrawInstanced(commandBuffer, items);
}

void Renderer::DrawInstanced(vk::CommandBuffer& commandBuffer, const std::vector<const DrawItem*>& items)
{
	// Each run of the same model is one draw, an instance per object: its slots go in the draw list,
	// and the draw's first instance says where they start.
	auto list = static_cast<uint32_t*>(m_graphics->m_drawListBuffersMapped[m_graphics->m_frameInFlightIndex]);
	for (size_t first = 0; first < items.size();)
	{
		uint32_t modelIndex = items[first]->modelIndex;
		size_t end = first + 1;
		while (end < items.size() && items[end]->modelIndex == modelIndex)
			end++;
		auto count = static_cast<uint32_t>(end - first);
		// A full list leaves the rest undrawn this frame rather than overrunning the buffer.
		if (m_drawCursor + count > MAX_DRAW_ENTRIES)
			return;

		uint32_t firstInstance = m_drawCursor;
		for (size_t i = first; i < end; i++)
			list[m_drawCursor++] = items[i]->slot;
		commandBuffer.drawIndexed(m_modelManger->m_indicesCounts[modelIndex - 1], count, m_modelManger->m_indicesOffsets[modelIndex - 1],
			static_cast<uint32_t>(m_modelManger->m_verticesOffsets[modelIndex - 1]), firstInstance);
		first = end;
	}
}

glm::vec2 Renderer::NextJitter(const vk::Extent2D& renderExtent, const vk::Extent2D& outputExtent)
{
	// Halton (2, 3) points spread evenly over the pixel. DLSS wants a longer cycle the more it
	// upscales, so every output pixel gets sampled.
	auto halton = [](uint32_t index, uint32_t base) {
		float result = 0.f;
		float fraction = 1.f;
		for (; index > 0; index /= base)
		{
			fraction /= static_cast<float>(base);
			result += fraction * static_cast<float>(index % base);
		}
		return result;
	};
	float scale = static_cast<float>(outputExtent.width) / static_cast<float>((std::max)(renderExtent.width, 1u));
	uint32_t phases = (std::max)(8u, static_cast<uint32_t>(std::ceil(8.f * scale * scale)));
	m_jitterIndex = m_jitterIndex % phases + 1;
	return { halton(m_jitterIndex, 2) - 0.5f, halton(m_jitterIndex, 3) - 0.5f };
}

void Renderer::RecordUpscale(vk::CommandBuffer& commandBuffer, const glm::vec2& jitter, bool reset)
{
	auto& output = m_graphics->m_upscaleOutput;
	vk::ImageSubresourceRange colorRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

	// DLSS writes all of the output each frame, so last frame's contents can go once the composite
	// is done reading them.
	vk::ImageMemoryBarrier toWrite {
		.srcAccessMask = vk::AccessFlagBits::eShaderRead,
		.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eGeneral,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = output.image,
		.subresourceRange = colorRange
	};
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eAllCommands, {},
		0, nullptr, 0, nullptr, 1, &toWrite);

	vk::Extent2D renderExtent = m_graphics->m_renderExtent;
	vk::Extent2D outputExtent = m_graphics->m_swapChainInfo.imageExtent;
	auto image = [](const Graphics::UpscaleImage& target, vk::Format format, VkImageAspectFlags aspect, vk::Extent2D size) {
		return DLSS::Image { static_cast<VkImage>(target.image), static_cast<VkImageView>(target.view), static_cast<VkFormat>(format),
			aspect, size.width, size.height };
	};

	DLSS::Inputs inputs;
	inputs.color = image(m_graphics->m_upscaleColor, m_graphics->m_upscaleColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, renderExtent);
	inputs.motionVectors = image(m_graphics->m_upscaleMotion, m_graphics->m_upscaleMotionFormat, VK_IMAGE_ASPECT_COLOR_BIT, renderExtent);
	inputs.depth = image(m_graphics->m_upscaleDepth, m_graphics->m_upscaleDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, renderExtent);
	inputs.output = image(output, m_graphics->m_upscaleColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, outputExtent);
	inputs.jitterX = jitter.x;
	inputs.jitterY = jitter.y;
	// The shader writes motion in screen UV; DLSS wants render pixels.
	inputs.motionScaleX = static_cast<float>(renderExtent.width);
	inputs.motionScaleY = static_cast<float>(renderExtent.height);
	inputs.frameTimeMs = m_frameTimeMs;
	inputs.reset = reset;
	m_graphics->m_dlss.Evaluate(static_cast<VkCommandBuffer>(commandBuffer), inputs);

	vk::ImageMemoryBarrier toRead {
		.srcAccessMask = vk::AccessFlagBits::eShaderWrite,
		.dstAccessMask = vk::AccessFlagBits::eShaderRead,
		.oldLayout = vk::ImageLayout::eGeneral,
		.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = output.image,
		.subresourceRange = colorRange
	};
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eFragmentShader, {},
		0, nullptr, 0, nullptr, 1, &toRead);
}

void Renderer::ResetTemporalHistory()
{
	m_resetHistory = true;
}

bool Renderer::ShadowsActive() const
{
	// No sun, no sun shadows: at night only the (unshadowed) lamps light the scene.
	glm::vec3 sun = m_lighting.sunColor;
	return m_shadowSettings.resolution > 0 && m_graphics->m_shadowResolution > 1 && (std::max)({ sun.r, sun.g, sun.b }) > 0.01f;
}

glm::mat4 Renderer::CascadeMatrix(const glm::vec3& cameraPosition, const glm::vec3& forward, float tanHalfFov, float aspect,
	float farDistance, uint32_t resolution, float& texelWorldSize) const
{
	// The smallest sphere around the view frustum from the eye out to farDistance. A sphere doesn't
	// change size as the camera turns, which keeps the texel size (and the snapping) steady.
	float tanHalfWidth = tanHalfFov * aspect;
	float spread = tanHalfFov * tanHalfFov + tanHalfWidth * tanHalfWidth;
	float centerDistance = (std::min)(farDistance, farDistance * (1.f + spread) * 0.5f);
	float cornerDistance = std::sqrt((farDistance - centerDistance) * (farDistance - centerDistance) + farDistance * farDistance * spread);
	float radius = (std::max)(centerDistance, cornerDistance);
	radius = std::ceil(radius * 8.f) / 8.f;

	glm::vec3 center = cameraPosition + forward * centerDistance;
	glm::vec3 towardSun = glm::normalize(m_lighting.sunDirection);
	glm::vec3 up = std::abs(towardSun.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);

	// Back far enough towards the sun to catch roofs and trees outside the sphere that shade it.
	float pullBack = radius + 80.f;
	glm::mat4 view = glm::lookAt(center + towardSun * pullBack, center, up);
	glm::mat4 projection = glm::ortho(-radius, radius, -radius, radius, 0.f, pullBack + radius);

	// Snap the world origin to a texel so the whole map moves in whole texels.
	glm::vec4 origin = projection * view * glm::vec4(0.f, 0.f, 0.f, 1.f);
	glm::vec2 texels = glm::vec2(origin) * (static_cast<float>(resolution) * 0.5f);
	glm::vec2 snap = (glm::round(texels) - texels) * (2.f / static_cast<float>(resolution));
	projection[3][0] += snap.x;
	projection[3][1] += snap.y;

	texelWorldSize = 2.f * radius / static_cast<float>(resolution);
	return projection * view;
}

void Renderer::WriteSceneUniforms(const Camera& camera, float aspect, SceneUniforms& scene) const
{
	glm::vec3 cameraPosition = camera.GetPosition();
	glm::vec3 forward = camera.GetRotation() * glm::vec3(0.f, 0.f, -1.f);
	float tanHalfFov = std::tan(glm::radians(camera.GetFov()) * 0.5f);

	bool shadows = ShadowsActive();
	uint32_t resolution = (std::max)(m_graphics->m_shadowResolution, 1u);
	float distance = (std::max)(m_shadowSettings.distance, 5.f);
	// Fine detail close up (engine parts on the bench), coarse out to the shadow distance.
	float splits[SHADOW_CASCADES] = { std::clamp(distance * 0.15f, 3.f, 7.f), distance };
	float texelSizes[SHADOW_CASCADES] = {};
	for (int cascade = 0; cascade < SHADOW_CASCADES; cascade++)
		scene.lightViewProj[cascade] = CascadeMatrix(cameraPosition, forward, tanHalfFov, aspect, splits[cascade], resolution, texelSizes[cascade]);

	scene.shadowParams = { texelSizes[0], texelSizes[1], shadows ? 1.f : 0.f, 1.f / static_cast<float>(resolution) };
	scene.cameraPosition = glm::vec4(cameraPosition, m_lighting.exposure);
	scene.sunDirection = glm::vec4(glm::normalize(m_lighting.sunDirection), 0.f);
	scene.sunColor = glm::vec4(m_lighting.sunColor, 0.f);
	scene.skyZenith = glm::vec4(m_lighting.skyZenith, 0.f);
	scene.skyHorizon = glm::vec4(m_lighting.skyHorizon, 0.f);
	scene.groundColor = glm::vec4(m_lighting.groundColor, 0.f);
	int interiors = (std::min)(static_cast<int>(m_lighting.interiors.size()), MAX_INTERIORS);
	scene.interiorAmbient = glm::vec4(m_lighting.interiorAmbient, static_cast<float>(interiors));
	for (int i = 0; i < interiors; i++)
	{
		scene.interiorMin[i] = glm::vec4(m_lighting.interiors[i].min, 0.f);
		scene.interiorMax[i] = glm::vec4(m_lighting.interiors[i].max, 0.f);
	}
	scene.fogColor = glm::vec4(m_lighting.fogColor, m_lighting.fogDensity);

	int count = (std::min)(static_cast<int>(m_lighting.pointLights.size()), MAX_POINT_LIGHTS);
	scene.lightCount = { count, 0, 0, 0 };

	// Lamp shadows follow the shadow quality setting (off turns them off too), sun or no sun.
	uint32_t lampResolution = m_graphics->m_lightShadowResolution;
	bool lampShadows = m_shadowSettings.resolution > 0 && lampResolution > 1;
	int shadowLayers = 0;
	for (int i = 0; i < MAX_POINT_LIGHTS; i++)
		scene.lightShadow[i] = { -1.f, 0.f, 0.f, 0.f };

	for (int i = 0; i < count; i++)
	{
		const auto& light = m_lighting.pointLights[i];
		scene.lightPositionRange[i] = glm::vec4(light.position, light.range);
		scene.lightColorZone[i] = glm::vec4(light.color, static_cast<float>(light.zone));

		if (!lampShadows || !light.castsShadows || shadowLayers >= MAX_SHADOWED_LIGHTS)
			continue;
		// A wide perspective view from the lamp along its shadow direction, out to its range.
		float cone = glm::radians(std::clamp(light.shadowConeDegrees, 10.f, 170.f));
		glm::vec3 direction = glm::normalize(light.shadowDirection);
		glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
		glm::mat4 view = glm::lookAt(light.position, light.position + direction, up);
		glm::mat4 projection = glm::perspective(cone, 1.f, 0.05f, (std::max)(light.range, 0.1f));

		int layer = shadowLayers++;
		scene.lightShadowViewProj[layer] = projection * view;
		// How wide one shadow texel is per metre of distance from the lamp, for the shader's offset.
		float texelPerMetre = 2.f * std::tan(cone * 0.5f) / static_cast<float>(lampResolution);
		scene.lightShadow[i] = { static_cast<float>(layer), texelPerMetre, 1.f / static_cast<float>(lampResolution), 0.f };
	}
}

void Renderer::SetSceneLighting(const SceneLighting& lighting)
{
	m_lighting = lighting;
}

const SceneLighting& Renderer::GetSceneLighting() const
{
	return m_lighting;
}

void Renderer::SetShadowSettings(const ShadowSettings& settings)
{
	m_shadowSettings = settings;
	m_graphics->CreateShadowMap((std::max)(settings.resolution, 1u));
}

const ShadowSettings& Renderer::GetShadowSettings() const
{
	return m_shadowSettings;
}

void Renderer::RenderFrame()
{
	auto now = std::chrono::steady_clock::now();
	if (m_lastFrameTime != std::chrono::steady_clock::time_point {})
		m_frameTimeMs = std::chrono::duration<float, std::milli>(now - m_lastFrameTime).count();
	m_lastFrameTime = now;

	m_performanceManager->StartCPUWaitForGPUTimer();
	if (m_graphics->m_logicalDevice.waitForFences(1, &m_graphics->m_frameInFlightFences[m_graphics->m_frameInFlightIndex], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("waitForFences failed!");
	m_performanceManager->EndCPUWaitForGPUTimer();

	uint32_t imageIndex = 0;
	vk::Result result = m_graphics->m_logicalDevice.acquireNextImageKHR(m_graphics->m_swapChain, UINT64_MAX,
		m_graphics->m_imageAvailableSemaphores[m_graphics->m_frameInFlightIndex], VK_NULL_HANDLE, &imageIndex);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		m_graphics->RecreateSwapChain();
		return;
	}
	else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	if (m_graphics->m_logicalDevice.resetFences(1, &m_graphics->m_frameInFlightFences[m_graphics->m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("resetFences failed!");

	m_performanceManager->StartGraphicsCommandBufferTimer();
	auto commandBuffer = RecordCommandBuffer(imageIndex);
	m_performanceManager->EndGraphicsCommandBufferTimer();

	commandBuffer.nextSubpass(vk::SubpassContents::eInline);

	m_performanceManager->StartIMGUICommandBufferTimer();
	m_imguiManager->RecordCommandBuffer(commandBuffer);
	m_performanceManager->EndIMGUICommandBufferTimer();

	commandBuffer.endRenderPass();
	commandBuffer.end();

	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submitInfo = {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_graphics->m_imageAvailableSemaphores[m_graphics->m_frameInFlightIndex],
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_graphics->m_mainRenderFinishedSemaphores[imageIndex]
	}; 
	
	if (m_graphics->m_queueGraphics.submit(1, &submitInfo, m_graphics->m_frameInFlightFences[m_graphics->m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit draw command buffer!");

	vk::PresentInfoKHR presentInfo = {
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_graphics->m_mainRenderFinishedSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &m_graphics->m_swapChain,
		.pImageIndices = &imageIndex,
		.pResults = nullptr
	};
	result = m_graphics->m_queueGraphics.presentKHR(&presentInfo);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_graphics->m_framebufferResized)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		m_graphics->RecreateSwapChain();
		m_graphics->m_framebufferResized = false;
	}
	else if (result != vk::Result::eSuccess)
		throw std::runtime_error("failed to present swap chain image!");

	m_graphics->m_frameInFlightIndex = ++m_graphics->m_frameInFlightIndex % m_graphics->m_maxFramesInFlight;
}

Renderer::Renderer()
{
	LOG_INFO("Initalizing graphics...");

	m_graphics = Graphics::Get();
	m_textureManager = TextureManager::Get();
	m_modelManger = ModelManager::Get();
	m_objectManager = ObjectManager::Get();
	m_cameraManager = CameraManager::Get();
	m_imguiManager = IMGUIManager::Get();
	m_performanceManager = PerformanceManager::Get();

	auto textureDescriptors = m_textureManager->CreateTextures();
	m_modelManger->CreateModels();

	m_graphics->CreateInstanceBuffers();

	m_graphics->CreateDescriptorPool(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSetLayout(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSets(textureDescriptors);
	m_graphics->CreateSceneResources();

	m_graphics->CreateGraphicsPipeline();
	m_graphics->CreateUpscaleResources();
	m_graphics->CreateDepthResources();
	m_graphics->CreateFrameBuffers();
	m_graphics->CreateSyncObjects();

	LOG_SUCCESS("Graphics initialized successfully!");
}

Renderer::~Renderer()
{
}
