#include "Graphics.h"

// DLSS: the render targets the scene is drawn into at the render size, the scene pipelines that
// draw into them (with motion vectors), and the full-screen pass that puts DLSS's output on the
// swap chain before the UI is drawn over it.

void Graphics::CreateUpscaleResources()
{
	if (!m_dlss.IsAvailable())
		return;

	LOG_INFO("Creating DLSS resources...");
	CreateUpscaleRenderPass();
	CreateScenePipelines(m_upscaleRenderPass, vk::SampleCountFlagBits::e1, "Engine/Graphics/Shaders/Compiled/vert_mv.spv",
		"Engine/Graphics/Shaders/Compiled/frag_mv.spv", 2, m_upscalePipeline, m_upscaleTransparentPipeline);
	CreateCompositePipeline();
	LOG_SUCCESS("DLSS resources created!");
}

void Graphics::CreateUpscaleRenderPass()
{
	// Everything ends up read by DLSS, so it's all stored and left ready to sample.
	auto attachment = [](vk::Format format) {
		return vk::AttachmentDescription {
			.flags = {},
			.format = format,
			.samples = vk::SampleCountFlagBits::e1,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};
	};
	std::array<vk::AttachmentDescription, 3> attachments = {
		attachment(m_upscaleColorFormat),
		attachment(m_upscaleMotionFormat),
		attachment(m_upscaleDepthFormat)
	};
	std::array<vk::AttachmentReference, 2> colorRefs = { {
		{ .attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal },
		{ .attachment = 1, .layout = vk::ImageLayout::eColorAttachmentOptimal }
	} };
	vk::AttachmentReference depthRef {
		.attachment = 2,
		.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	vk::SubpassDescription subpass {
		.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size()),
		.pColorAttachments = colorRefs.data(),
		.pDepthStencilAttachment = &depthRef
	};

	// Last frame's DLSS has to be done reading before these are drawn over, and this frame's
	// drawing has to land before DLSS reads it.
	std::array<vk::SubpassDependency, 2> dependencies = { {
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = vk::PipelineStageFlagBits::eAllCommands,
			.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests
				| vk::PipelineStageFlagBits::eLateFragmentTests,
			.srcAccessMask = vk::AccessFlagBits::eShaderRead,
			.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
				| vk::AccessFlagBits::eDepthStencilAttachmentRead
		},
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests,
			.dstStageMask = vk::PipelineStageFlagBits::eAllCommands,
			.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			.dstAccessMask = vk::AccessFlagBits::eShaderRead
		}
	} };

	vk::RenderPassCreateInfo renderPassInfo {
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = static_cast<uint32_t>(dependencies.size()),
		.pDependencies = dependencies.data()
	};
	if (m_logicalDevice.createRenderPass(&renderPassInfo, nullptr, &m_upscaleRenderPass) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS render pass!");
}

void Graphics::CreateCompositePipeline()
{
	vk::DescriptorSetLayoutBinding binding {
		.binding = 0,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = 1,
		.stageFlags = vk::ShaderStageFlagBits::eFragment,
		.pImmutableSamplers = nullptr
	};
	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.bindingCount = 1,
		.pBindings = &binding
	};
	if (m_logicalDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &m_compositeSetLayout) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS composite descriptor set layout!");

	vk::DescriptorPoolSize poolSize {
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = 1
	};
	vk::DescriptorPoolCreateInfo poolInfo {
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &poolSize
	};
	if (m_logicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_compositeDescriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS composite descriptor pool!");

	vk::DescriptorSetAllocateInfo allocInfo {
		.descriptorPool = m_compositeDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_compositeSetLayout
	};
	if (m_logicalDevice.allocateDescriptorSets(&allocInfo, &m_compositeDescriptorSet) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to allocate the DLSS composite descriptor set!");

	// The output is already the window's size: one texel per pixel.
	vk::SamplerCreateInfo samplerInfo {
		.magFilter = vk::Filter::eNearest,
		.minFilter = vk::Filter::eNearest,
		.mipmapMode = vk::SamplerMipmapMode::eNearest,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge,
		.maxLod = 0.f
	};
	if (m_logicalDevice.createSampler(&samplerInfo, nullptr, &m_compositeSampler) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS composite sampler!");

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
		.setLayoutCount = 1,
		.pSetLayouts = &m_compositeSetLayout
	};
	if (m_logicalDevice.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_compositePipelineLayout) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS composite pipeline layout!");

	vk::ShaderModule vertexModule = LoadShaderModule("Engine/Graphics/Shaders/Compiled/composite_vert.spv");
	vk::ShaderModule fragmentModule = LoadShaderModule("Engine/Graphics/Shaders/Compiled/composite_frag.spv");
	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = { {
		{ .stage = vk::ShaderStageFlagBits::eVertex, .module = vertexModule, .pName = "main" },
		{ .stage = vk::ShaderStageFlagBits::eFragment, .module = fragmentModule, .pName = "main" }
	} };

	// One triangle covering the screen, made up in the vertex shader.
	vk::PipelineVertexInputStateCreateInfo vertexInput {};
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly {
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};
	vk::PipelineViewportStateCreateInfo viewport {
		.viewportCount = 1,
		.scissorCount = 1
	};
	vk::PipelineRasterizationStateCreateInfo rasterizer {
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = vk::CullModeFlagBits::eNone,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.f
	};
	// The swap chain pass may still be multisampled (MSAA is only chosen at startup).
	vk::PipelineMultisampleStateCreateInfo multisampling {
		.rasterizationSamples = m_MSAALevel,
		.sampleShadingEnable = vk::False
	};
	vk::PipelineDepthStencilStateCreateInfo depthStencil {
		.depthTestEnable = vk::False,
		.depthWriteEnable = vk::False,
		.depthCompareOp = vk::CompareOp::eAlways,
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False,
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f
	};
	vk::PipelineColorBlendAttachmentState blendAttachment {
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};
	vk::PipelineColorBlendStateCreateInfo colorBlend {
		.logicOpEnable = vk::False,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment
	};
	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState {
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	vk::GraphicsPipelineCreateInfo pipelineInfo {
		.stageCount = static_cast<uint32_t>(stages.size()),
		.pStages = stages.data(),
		.pVertexInputState = &vertexInput,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlend,
		.pDynamicState = &dynamicState,
		.layout = m_compositePipelineLayout,
		.renderPass = m_renderPass,
		.subpass = 0
	};
	if (m_logicalDevice.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_compositePipeline) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS composite pipeline!");

	m_logicalDevice.destroyShaderModule(vertexModule, nullptr);
	m_logicalDevice.destroyShaderModule(fragmentModule, nullptr);
}

void Graphics::CreateUpscaleTargets()
{
	// The old targets and DLSS's history may still be in use by a frame in flight.
	m_logicalDevice.waitIdle();
	CleanupUpscaleTargets();
	if (m_dlssMode == DLSSMode::OFF || !m_dlss.IsAvailable() || !m_upscaleRenderPass)
		return;

	vk::Extent2D output = m_swapChainInfo.imageExtent;
	uint32_t renderWidth = 0, renderHeight = 0;
	if (!m_dlss.GetRenderSize(m_dlssMode, output.width, output.height, renderWidth, renderHeight))
	{
		LOG_WARNING("DLSS has no render size for this mode and window, drawing without it.");
		return;
	}
	LOG_INFO(std::format("DLSS: rendering at {}x{}, output {}x{}", renderWidth, renderHeight, output.width, output.height));

	auto createTarget = [&](UpscaleImage& target, uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage,
		vk::ImageAspectFlags aspect) {
		target.image = CreateImage(width, height, 1, vk::SampleCountFlagBits::e1, format, vk::ImageTiling::eOptimal, usage,
			vk::MemoryPropertyFlagBits::eDeviceLocal, target.memory);
		target.view = CreateImageView(target.image, format, aspect, 1);
	};
	createTarget(m_upscaleColor, renderWidth, renderHeight, m_upscaleColorFormat,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor);
	createTarget(m_upscaleMotion, renderWidth, renderHeight, m_upscaleMotionFormat,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor);
	createTarget(m_upscaleDepth, renderWidth, renderHeight, m_upscaleDepthFormat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eDepth);
	createTarget(m_upscaleOutput, output.width, output.height, m_upscaleColorFormat,
		vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst, vk::ImageAspectFlagBits::eColor);

	std::array<vk::ImageView, 3> attachments = { m_upscaleColor.view, m_upscaleMotion.view, m_upscaleDepth.view };
	vk::FramebufferCreateInfo framebufferInfo {
		.renderPass = m_upscaleRenderPass,
		.attachmentCount = static_cast<uint32_t>(attachments.size()),
		.pAttachments = attachments.data(),
		.width = renderWidth,
		.height = renderHeight,
		.layers = 1
	};
	if (m_logicalDevice.createFramebuffer(&framebufferInfo, nullptr, &m_upscaleFramebuffer) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create the DLSS framebuffer!");

	vk::DescriptorImageInfo outputInfo {
		.sampler = m_compositeSampler,
		.imageView = m_upscaleOutput.view,
		.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	};
	vk::WriteDescriptorSet write {
		.dstSet = m_compositeDescriptorSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = vk::DescriptorType::eCombinedImageSampler,
		.pImageInfo = &outputInfo
	};
	m_logicalDevice.updateDescriptorSets(1, &write, 0, nullptr);

	auto commandBuffer = BeginSingleTimeCommands();
	bool created = m_dlss.CreateFeature(commandBuffer, m_dlssMode, renderWidth, renderHeight, output.width, output.height);
	EndSingleTimeCommands(commandBuffer);
	if (!created)
	{
		CleanupUpscaleTargets();
		return;
	}

	m_renderExtent = { renderWidth, renderHeight };
	m_upscaling = true;
	m_upscaleHistoryLost = true;
}

void Graphics::CleanupUpscaleTargets()
{
	m_dlss.ReleaseFeature();
	m_upscaling = false;
	m_renderExtent = {};

	if (m_upscaleFramebuffer)
		m_logicalDevice.destroyFramebuffer(m_upscaleFramebuffer, nullptr);
	m_upscaleFramebuffer = nullptr;

	for (UpscaleImage* target : { &m_upscaleColor, &m_upscaleMotion, &m_upscaleDepth, &m_upscaleOutput })
	{
		if (target->view)
			m_logicalDevice.destroyImageView(target->view, nullptr);
		if (target->image)
			m_logicalDevice.destroyImage(target->image, nullptr);
		if (target->memory)
			m_logicalDevice.freeMemory(target->memory, nullptr);
		*target = {};
	}
}

void Graphics::CleanupUpscaleResources()
{
	CleanupUpscaleTargets();
	m_logicalDevice.destroyPipeline(m_compositePipeline, nullptr);
	m_logicalDevice.destroyPipelineLayout(m_compositePipelineLayout, nullptr);
	m_logicalDevice.destroySampler(m_compositeSampler, nullptr);
	m_logicalDevice.destroyDescriptorPool(m_compositeDescriptorPool, nullptr);
	m_logicalDevice.destroyDescriptorSetLayout(m_compositeSetLayout, nullptr);
	m_logicalDevice.destroyPipeline(m_upscalePipeline, nullptr);
	m_logicalDevice.destroyPipeline(m_upscaleTransparentPipeline, nullptr);
	m_logicalDevice.destroyRenderPass(m_upscaleRenderPass, nullptr);
	m_dlss.Shutdown();
}

bool Graphics::IsDLSSAvailable() const
{
	return m_dlss.IsAvailable() && m_upscaleRenderPass;
}

const std::string& Graphics::GetDLSSStatus() const
{
	return m_dlss.GetStatus();
}

void Graphics::SetDLSSMode(DLSSMode mode)
{
	if (!IsDLSSAvailable())
		mode = DLSSMode::OFF;
	if (mode == m_dlssMode)
		return;

	m_dlssMode = mode;
	if (mode == DLSSMode::OFF)
	{
		m_logicalDevice.waitIdle();
		CleanupUpscaleTargets();
	}
	else
		CreateUpscaleTargets();
}

DLSSMode Graphics::GetDLSSMode() const
{
	return m_dlssMode;
}

bool Graphics::IsUpscaling() const
{
	return m_upscaling;
}

vk::Extent2D Graphics::GetRenderExtent() const
{
	return m_upscaling ? m_renderExtent : m_swapChainInfo.imageExtent;
}
