#include "Renderer.h"

std::shared_ptr<Renderer> Renderer::_instance = nullptr;

std::shared_ptr<Renderer> Renderer::Get()
{
	if (_instance == nullptr)
		_instance = std::shared_ptr<Renderer>(new Renderer());

	return _instance;
}

void Renderer::RenderFrame()
{
	if (m_graphics->m_logicalDevice.waitForFences(1, &m_graphics->m_frameInFlightFences[m_frameInFlightIndex], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("waitForFences failed!");

	uint32_t imageIndex = 0;
	vk::Result result = m_graphics->m_logicalDevice.acquireNextImageKHR(m_graphics->m_swapChain, UINT64_MAX,
		m_graphics->m_imageAvailableSemaphores[m_frameInFlightIndex], VK_NULL_HANDLE, &imageIndex);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		LOG_WARNING("Swap chain out of date, recreating swap chain...");
		m_graphics->RecreateSwapChain();
		return;
	}
	else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		throw std::runtime_error("failed to acquire swap chain image!");

	if (m_graphics->m_logicalDevice.resetFences(1, &m_graphics->m_frameInFlightFences[m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("resetFences failed!");

	auto commandBuffer = m_graphics->m_commandBuffers[m_frameInFlightIndex];
	commandBuffer.reset();

	vk::CommandBufferBeginInfo beginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = nullptr
	};

	if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess)
		throw std::runtime_error("Failed begin on command buffer!");

	vk::ClearValue clearColor{
		.color = std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
	vk::ClearValue clearDepth{
		.depthStencil = { 1.f, 0 }
	};
	std::vector<vk::ClearValue> clearValues = { clearColor, clearDepth };
	vk::Rect2D renderArea{
		.offset = {0, 0},
		.extent = m_graphics->m_swapChainInfo.imageExtent
	};
	vk::RenderPassBeginInfo rpBeginInfo{
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

	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBuffer ubo{};
	ubo.model = glm::rotate(glm::mat4(1.f), duration * glm::radians(-90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.model *= glm::rotate(ubo.model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
	ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.proj = glm::perspective(glm::radians(45.f),
		static_cast<float>(m_graphics->m_swapChainCapabilities.currentExtent.width) /
		static_cast<float>(m_graphics->m_swapChainCapabilities.currentExtent.height),
		0.1f, 10.f);
	ubo.proj[1][1] *= -1;
	memcpy(m_graphics->m_uniformBuffersMapped[m_frameInFlightIndex], &ubo, sizeof(ubo));
	std::vector<vk::DescriptorSet> descriptorSets = { m_graphics->m_descriptorSetsUBO[m_frameInFlightIndex],
		m_graphics->m_descriptorSetSampler };
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics->m_pipelineLayout,
		0, static_cast<uint32_t>(descriptorSets.size()),
		descriptorSets.data(), 0, nullptr);
	uint32_t textureIndex = 0;
	commandBuffer.pushConstants(m_graphics->m_pipelineLayout, vk::ShaderStageFlagBits::eFragment,
		0, sizeof(uint32_t), &textureIndex);
	
	vk::DeviceSize descriptorSetBindingOffset = 0;
	commandBuffer.bindVertexBuffers(0, 1, &m_modelManger->m_vertexBuffer, &descriptorSetBindingOffset);
	commandBuffer.bindIndexBuffer(m_modelManger->m_indexBuffer, m_modelManger->m_indexOffsets[0] * sizeof(uint32_t),
		vk::IndexType::eUint32);

	commandBuffer.drawIndexed(m_modelManger->m_indexSizes[0], 1, m_modelManger->m_indexOffsets[0], static_cast<uint32_t>(m_modelManger->m_vertexOffsets[0]), 0);

	commandBuffer.endRenderPass();

	commandBuffer.end();

	std::vector<vk::Semaphore> waitSemaphores = { m_graphics->m_imageAvailableSemaphores[m_frameInFlightIndex] };
	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	std::vector<vk::Semaphore> signalSemaphores = { m_graphics->m_renderFinishedSemaphores[imageIndex] };

	vk::SubmitInfo submitInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores.data(),
		.pWaitDstStageMask = waitStages.data(),
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores.data()
	};

	if (m_graphics->m_queueGraphics.submit(1, &submitInfo, m_graphics->m_frameInFlightFences[m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit draw command buffer!");

	std::vector<vk::SwapchainKHR> swapChains = { m_graphics->m_swapChain };

	vk::PresentInfoKHR presentInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores.data(),
		.swapchainCount = 1,
		.pSwapchains = swapChains.data(),
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

	m_frameInFlightIndex = ++m_frameInFlightIndex % m_maxFramesInFlight;
}

Renderer::Renderer()
{
	LOG_INFO("Initalizing graphics...");

	m_graphics = Graphics::Get();
	m_textureManager = TextureManager::Get();
	m_modelManger = ModelManager::Get();

	m_graphics->SetupVulkan();

	pTexture texture = m_textureManager->CreateTexture({"Sheep", "Engine/Graphics/Resources/sheep.png"});
	m_textureManager->AddTexture(texture);
	m_textureManager->CreateTextures();

	Model model = { "Sheep", "Engine/Graphics/Resources/sheep.obj" };
	m_modelManger->AddModel(model);
	m_modelManger->CreateModels();

	ObjectManager::Get();

	m_graphics->CreateUniformBuffers();

	m_graphics->CreateDescriptorPool(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSetLayout(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSets(m_textureManager->GetNumberOfTextures(), m_textureManager->m_textureDescriptors);

	m_graphics->CreateGraphicsPipeline();

	m_graphics->CreateDepthResources();
	m_graphics->CreateFrameBuffers();
	m_graphics->CreateSyncObjects();

	LOG_SUCCESS("Graphics initialized successfully!");
}

Renderer::~Renderer()
{
}
