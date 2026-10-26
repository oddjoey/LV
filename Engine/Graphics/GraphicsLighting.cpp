#include "Graphics.h"

#include "Lighting.h"

// Scene lighting resources, sun shadow maps and the video options that can change at runtime.

void Graphics::CreateSceneResources()
{
	LOG_INFO("Creating scene lighting resources...");

	// Set 2: the scene uniform block and the shadow map.
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { {
		{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			.pImmutableSamplers = nullptr
		},
		{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment,
			.pImmutableSamplers = nullptr
		}
	} };
	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};
	if (m_logicalDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &m_sceneDescriptorSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create descriptor set layout for the scene!");

	m_sceneUniformBuffers = std::vector<vk::Buffer>(m_maxFramesInFlight);
	m_sceneUniformBuffersMemory = std::vector<vk::DeviceMemory>(m_maxFramesInFlight);
	m_sceneUniformBuffersMapped = std::vector<void*>(m_maxFramesInFlight);
	for (int i = 0; i < m_maxFramesInFlight; i++)
	{
		m_sceneUniformBuffers[i] = CreateBuffer(sizeof(SceneUniforms), vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_sceneUniformBuffersMemory[i], true);
		if (m_logicalDevice.mapMemory(m_sceneUniformBuffersMemory[i], 0, sizeof(SceneUniforms), {}, &m_sceneUniformBuffersMapped[i]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to map the scene uniform buffer!");
		SceneUniforms empty = {};
		memcpy(m_sceneUniformBuffersMapped[i], &empty, sizeof(empty));
	}

	// Hardware depth comparison, filtered: each lookup is already a 2x2 PCF.
	vk::SamplerCreateInfo samplerInfo {
		.flags = {},
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToBorder,
		.addressModeV = vk::SamplerAddressMode::eClampToBorder,
		.addressModeW = vk::SamplerAddressMode::eClampToBorder,
		.mipLodBias = 0.f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.f,
		.compareEnable = vk::True,
		.compareOp = vk::CompareOp::eLessOrEqual,
		.minLod = 0.f,
		.maxLod = 0.f,
		.borderColor = vk::BorderColor::eFloatOpaqueWhite,
		.unnormalizedCoordinates = vk::False,
	};
	if (m_logicalDevice.createSampler(&samplerInfo, nullptr, &m_shadowSampler) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow sampler!");
	m_manageSamplers.push_back(m_shadowSampler);

	std::vector<vk::DescriptorSetLayout> layouts(m_maxFramesInFlight, m_sceneDescriptorSetLayout);
	vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = m_graphicsDescriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(m_maxFramesInFlight),
		.pSetLayouts = layouts.data()
	};
	m_sceneDescriptorSets = std::vector<vk::DescriptorSet>(m_maxFramesInFlight);
	if (m_logicalDevice.allocateDescriptorSets(&allocInfo, m_sceneDescriptorSets.data()) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate scene descriptor sets!");

	for (int i = 0; i < m_maxFramesInFlight; i++)
	{
		vk::DescriptorBufferInfo bufferInfo {
			.buffer = m_sceneUniformBuffers[i],
			.offset = 0,
			.range = sizeof(SceneUniforms)
		};
		vk::WriteDescriptorSet write {
			.dstSet = m_sceneDescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo
		};
		m_logicalDevice.updateDescriptorSets(1, &write, 0, nullptr);
	}

	CreateShadowRenderPass();
	CreateShadowPipeline();
	// A token map until the game picks a quality, so the descriptor is never empty.
	CreateShadowMap(1);

	LOG_SUCCESS("Scene lighting resources created!");
}

void Graphics::CreateShadowRenderPass()
{
	vk::AttachmentDescription depthAttachment {
		.flags = {},
		.format = m_shadowFormat,
		.samples = vk::SampleCountFlagBits::e1,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
		.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
		.initialLayout = vk::ImageLayout::eUndefined,
		.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::AttachmentReference depthRef {
		.attachment = 0,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::SubpassDescription subpass {
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = 0,
		.pDepthStencilAttachment = &depthRef
	};

	// Last frame's lighting pass must be done reading before this one writes, and this one must be
	// done writing before the lighting pass samples it.
	std::array<vk::SubpassDependency, 2> dependencies = { {
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = vk::PipelineStageFlagBits::eFragmentShader,
			.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
			.srcAccessMask = vk::AccessFlagBits::eShaderRead,
			.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			.dependencyFlags = vk::DependencyFlagBits::eByRegion
		},
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = vk::PipelineStageFlagBits::eLateFragmentTests,
			.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader,
			.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			.dstAccessMask = vk::AccessFlagBits::eShaderRead,
			.dependencyFlags = vk::DependencyFlagBits::eByRegion
		}
	} };

	vk::RenderPassCreateInfo renderPassInfo {
		.attachmentCount = 1,
		.pAttachments = &depthAttachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = static_cast<uint32_t>(dependencies.size()),
		.pDependencies = dependencies.data()
	};
	if (m_logicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_shadowRenderPass) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the shadow render pass!");
}

void Graphics::CreateShadowPipeline()
{
	auto shaderCode = Utility::ReadFile("Engine/Graphics/Shaders/Compiled/shadow.spv");
	vk::ShaderModuleCreateInfo shaderInfo {
		.codeSize = shaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data())
	};
	vk::ShaderModule shaderModule = {};
	if (m_logicalDevice.createShaderModule(&shaderInfo, nullptr, &shaderModule) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow shader module!");

	// Depth only: no fragment shader.
	vk::PipelineShaderStageCreateInfo stage {
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = shaderModule,
		.pName = "main"
	};

	vk::VertexInputBindingDescription binding {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex
	};
	vk::VertexInputAttributeDescription position {
		.location = 0,
		.binding = 0,
		.format = vk::Format::eR32G32B32Sfloat,
		.offset = offsetof(Vertex, position)
	};
	vk::PipelineVertexInputStateCreateInfo vertexInput {
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &position
	};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};
	vk::PipelineViewportStateCreateInfo viewport {
		.viewportCount = 1,
		.scissorCount = 1
	};
	// Both faces cast (thin walls and single-sided panels), with slope-scaled bias against acne.
	// Depth clamp keeps casters between the sun and the near plane from being cut off.
	vk::PipelineRasterizationStateCreateInfo rasterizer {
		.depthClampEnable = m_depthClampSupported ? vk::True : vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::True,
		.depthBiasConstantFactor = 1.25f,
		.depthBiasClamp = 0.f,
		.depthBiasSlopeFactor = 1.75f,
		.lineWidth = 1.f
	};
	vk::PipelineMultisampleStateCreateInfo multisampling {
		.rasterizationSamples = vk::SampleCountFlagBits::e1
	};
	vk::PipelineDepthStencilStateCreateInfo depthStencil {
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLessOrEqual,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f
	};
	vk::PipelineColorBlendStateCreateInfo colorBlend {
		.attachmentCount = 0
	};
	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState {
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	// The model matrix comes from the same dynamic uniform buffer as the main pass; the cascade's
	// light matrix is a push constant.
	vk::PushConstantRange pushConstant {
		.stageFlags = vk::ShaderStageFlagBits::eVertex,
		.offset = 0,
		.size = sizeof(glm::mat4)
	};
	vk::PipelineLayoutCreateInfo layoutInfo {
		.setLayoutCount = 1,
		.pSetLayouts = &m_dynamicUBODescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstant
	};
	if (m_logicalDevice.createPipelineLayout(&layoutInfo, nullptr, &m_shadowPipelineLayout) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow pipeline layout!");

	vk::GraphicsPipelineCreateInfo pipelineInfo {
		.stageCount = 1,
		.pStages = &stage,
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlend,
		.pDynamicState = &dynamicState,
		.layout = m_shadowPipelineLayout,
		.renderPass = m_shadowRenderPass,
		.subpass = 0
	};
	if (m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_shadowPipeline) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow pipeline!");

	m_logicalDevice.destroyShaderModule(shaderModule, nullptr);
}

void Graphics::CreateShadowMap(uint32_t resolution)
{
	resolution = (std::max)(resolution, 1u);
	if (resolution == m_shadowResolution && m_shadowImage)
		return;

	// The old map may still be in use by a frame in flight.
	m_logicalDevice.waitIdle();
	CleanupShadowMap();

	vk::ImageCreateInfo imageInfo {
		.imageType = vk::ImageType::e2D,
		.format = m_shadowFormat,
		.extent = { resolution, resolution, 1 },
		.mipLevels = 1,
		.arrayLayers = SHADOW_CASCADES,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined
	};
	if (m_logicalDevice.createImage(&imageInfo, nullptr, &m_shadowImage) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow map!");

	vk::MemoryRequirements requirements;
	m_logicalDevice.getImageMemoryRequirements(m_shadowImage, &requirements);
	vk::MemoryAllocateInfo allocInfo {
		.allocationSize = requirements.size,
		.memoryTypeIndex = FindMemoryTypeIndex(requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};
	if (m_logicalDevice.allocateMemory(&allocInfo, nullptr, &m_shadowImageMemory) != vk::Result::eSuccess)
		throw std::runtime_error("failed to allocate shadow map memory!");
	m_logicalDevice.bindImageMemory(m_shadowImage, m_shadowImageMemory, 0);

	vk::ImageViewCreateInfo arrayViewInfo {
		.image = m_shadowImage,
		.viewType = vk::ImageViewType::e2DArray,
		.format = m_shadowFormat,
		.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, SHADOW_CASCADES }
	};
	if (m_logicalDevice.createImageView(&arrayViewInfo, nullptr, &m_shadowArrayView) != vk::Result::eSuccess)
		throw std::runtime_error("failed to create the shadow map view!");

	m_shadowLayerViews = std::vector<vk::ImageView>(SHADOW_CASCADES);
	m_shadowFramebuffers = std::vector<vk::Framebuffer>(SHADOW_CASCADES);
	for (uint32_t layer = 0; layer < SHADOW_CASCADES; layer++)
	{
		vk::ImageViewCreateInfo layerViewInfo {
			.image = m_shadowImage,
			.viewType = vk::ImageViewType::e2D,
			.format = m_shadowFormat,
			.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, layer, 1 }
		};
		if (m_logicalDevice.createImageView(&layerViewInfo, nullptr, &m_shadowLayerViews[layer]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create a shadow cascade view!");

		vk::FramebufferCreateInfo framebufferInfo {
			.renderPass = m_shadowRenderPass,
			.attachmentCount = 1,
			.pAttachments = &m_shadowLayerViews[layer],
			.width = resolution,
			.height = resolution,
			.layers = 1
		};
		if (m_logicalDevice.createFramebuffer(&framebufferInfo, nullptr, &m_shadowFramebuffers[layer]) != vk::Result::eSuccess)
			throw std::runtime_error("failed to create a shadow framebuffer!");
	}

	// Ready to sample even before the first shadow pass (or when shadows are off).
	auto commandBuffer = BeginSingleTimeCommands();
	vk::ImageMemoryBarrier barrier {
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eShaderRead,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = m_shadowImage,
		.subresourceRange = { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, SHADOW_CASCADES }
	};
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr, 0, nullptr, 1, &barrier);
	EndSingleTimeCommands(commandBuffer);

	for (int i = 0; i < m_maxFramesInFlight; i++)
	{
		vk::DescriptorImageInfo imageInfoDesc {
			.sampler = m_shadowSampler,
			.imageView = m_shadowArrayView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};
		vk::WriteDescriptorSet write {
			.dstSet = m_sceneDescriptorSets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &imageInfoDesc
		};
		m_logicalDevice.updateDescriptorSets(1, &write, 0, nullptr);
	}

	m_shadowResolution = resolution;
}

void Graphics::CleanupShadowMap()
{
	for (auto framebuffer : m_shadowFramebuffers)
		m_logicalDevice.destroyFramebuffer(framebuffer, nullptr);
	for (auto view : m_shadowLayerViews)
		m_logicalDevice.destroyImageView(view, nullptr);
	m_shadowFramebuffers.clear();
	m_shadowLayerViews.clear();
	if (m_shadowArrayView)
		m_logicalDevice.destroyImageView(m_shadowArrayView, nullptr);
	if (m_shadowImage)
		m_logicalDevice.destroyImage(m_shadowImage, nullptr);
	if (m_shadowImageMemory)
		m_logicalDevice.freeMemory(m_shadowImageMemory, nullptr);
	m_shadowArrayView = nullptr;
	m_shadowImage = nullptr;
	m_shadowImageMemory = nullptr;
	m_shadowResolution = 0;
}

void Graphics::CleanupSceneResources()
{
	CleanupShadowMap();
	m_logicalDevice.destroyPipeline(m_shadowPipeline, nullptr);
	m_logicalDevice.destroyPipelineLayout(m_shadowPipelineLayout, nullptr);
	m_logicalDevice.destroyRenderPass(m_shadowRenderPass, nullptr);
	m_logicalDevice.destroyPipeline(m_pipelineTransparent, nullptr);
	m_logicalDevice.destroyDescriptorSetLayout(m_sceneDescriptorSetLayout, nullptr);
}

void Graphics::SetVSync(VSyncOptions vsync)
{
	auto settings = Settings::Get()->GetGraphicsSettings();
	if (settings.eVSync == vsync)
		return;
	settings.eVSync = vsync;
	Settings::Get()->SetGraphicsSettings(settings);
	m_framebufferResized = true;
}

void Graphics::SetDisplayMode(DisplayMode mode, uint32_t width, uint32_t height)
{
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* desktop = monitor ? glfwGetVideoMode(monitor) : nullptr;
	if (!desktop)
		mode = DisplayMode::WINDOWED;

	int currentWidth = 0, currentHeight = 0;
	glfwGetWindowSize(m_window, &currentWidth, &currentHeight);
	bool sameMode = mode == m_displayMode;
	if (sameMode && mode == DisplayMode::BORDERLESS)
		return;
	if (sameMode && currentWidth == static_cast<int>(width) && currentHeight == static_cast<int>(height))
		return;

	if (m_displayMode == DisplayMode::WINDOWED)
		glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);

	switch (mode)
	{
		case DisplayMode::WINDOWED:
		{
			// Keep the title bar on screen when coming back from fullscreen.
			int x = (std::max)(m_windowedX, 0);
			int y = (std::max)(m_windowedY, 32);
			if (desktop)
			{
				width = (std::min)(width, static_cast<uint32_t>(desktop->width));
				height = (std::min)(height, static_cast<uint32_t>(desktop->height));
				x = (std::min)(x, (std::max)(0, desktop->width - static_cast<int>(width)));
				y = (std::min)(y, (std::max)(32, desktop->height - static_cast<int>(height)));
			}
			glfwSetWindowMonitor(m_window, nullptr, x, y, static_cast<int>(width), static_cast<int>(height), GLFW_DONT_CARE);
			break;
		}
		case DisplayMode::BORDERLESS:
			// GLFW makes a borderless "windowed fullscreen" when the mode matches the desktop's.
			glfwSetWindowMonitor(m_window, monitor, 0, 0, desktop->width, desktop->height, desktop->refreshRate);
			break;
		case DisplayMode::FULLSCREEN:
			glfwSetWindowMonitor(m_window, monitor, 0, 0, static_cast<int>(width), static_cast<int>(height), GLFW_DONT_CARE);
			break;
	}

	m_displayMode = mode;
	m_framebufferResized = true;
}

DisplayMode Graphics::GetDisplayMode() const
{
	return m_displayMode;
}

std::vector<std::pair<uint32_t, uint32_t>> Graphics::GetDisplayResolutions() const
{
	std::vector<std::pair<uint32_t, uint32_t>> resolutions;
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	if (!monitor)
		return resolutions;

	int count = 0;
	const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
	for (int i = 0; i < count; i++)
	{
		std::pair<uint32_t, uint32_t> size = { static_cast<uint32_t>(modes[i].width), static_cast<uint32_t>(modes[i].height) };
		if (size.first >= 800 && std::find(resolutions.begin(), resolutions.end(), size) == resolutions.end())
			resolutions.push_back(size);
	}
	std::sort(resolutions.begin(), resolutions.end());
	return resolutions;
}

vk::SampleCountFlagBits Graphics::GetMaxMSAA() const
{
	return m_MSAAMax;
}
