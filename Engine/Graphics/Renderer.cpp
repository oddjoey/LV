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
	if (m_graphics->m_logicalDevice.waitForFences(1, &m_graphics->m_frameInFlightFences[m_graphics->m_frameInFlightIndex], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("waitForFences failed!");

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

	auto commandBuffer = m_graphics->m_commandBuffers[m_graphics->m_frameInFlightIndex];
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

	auto camera = m_cameraManager->GetActiveCamera();
	static UniformBuffer ubo {};
	ubo.view = camera->GetViewMatrix();
	ubo.proj = glm::perspective(camera->GetFov(),
		static_cast<float>(m_graphics->m_swapChainCapabilities.currentExtent.width) /
		static_cast<float>(m_graphics->m_swapChainCapabilities.currentExtent.height),
		camera->GetNearPlane(), camera->GetFarPlane());
	ubo.proj[1][1] *= -1;

	std::vector<vk::DescriptorSet> descriptorSets { 
		m_graphics->m_descriptorSetsDynamicUBO[m_graphics->m_frameInFlightIndex],
		m_graphics->m_descriptorSetSampler 
	};
	vk::DeviceSize descriptorSetBindingOffset = 0;
	commandBuffer.bindVertexBuffers(0, 1, &m_modelManger->m_vertexBuffer, &descriptorSetBindingOffset);
	commandBuffer.bindIndexBuffer(m_modelManger->m_indexBuffer, 0, vk::IndexType::eUint32);
	for (auto& objectList : m_objectManager->GetObjects())
	{
		uint32_t modelIndex = objectList.first;

		if (!modelIndex)
			continue;

		for (auto& object : objectList.second)
		{
			glm::mat4 translation = glm::translate(glm::mat4(1.f), object->GetPosition());
			auto objRotation = object->GetRotation();
			glm::mat4 rotation = glm::rotate(glm::mat4(1.f), glm::radians(objRotation.x), glm::vec3(1.f, 0.f, 0.f));
			rotation = glm::rotate(rotation, glm::radians(objRotation.y), glm::vec3(0.f, 1.f, 0.f));
			rotation = glm::rotate(rotation, glm::radians(objRotation.z), glm::vec3(0.f, 0.f, 1.f));
			ubo.model = rotation * translation;

			uint32_t dynamicOffset = (object->GetIndex() - 1) * static_cast<uint32_t>(m_graphics->m_alignedUBOSize);
			uint8_t* dst = reinterpret_cast<uint8_t*>(m_graphics->m_uniformBuffersMapped[m_graphics->m_frameInFlightIndex]) + dynamicOffset;
			memcpy(dst, &ubo, sizeof(ubo));
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphics->m_pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 1, &dynamicOffset);

			uint32_t textureIndex = object->GetTextureIndex();
			commandBuffer.pushConstants(m_graphics->m_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &textureIndex);

			commandBuffer.drawIndexed(m_modelManger->m_indexSizes[modelIndex - 1], 1, m_modelManger->m_indexOffsets[modelIndex - 1], static_cast<uint32_t>(m_modelManger->m_vertexOffsets[modelIndex - 1]), 0);
		}
	}

	commandBuffer.endRenderPass();
	commandBuffer.end();

	std::vector<vk::Semaphore> waitSemaphores = { m_graphics->m_imageAvailableSemaphores[m_graphics->m_frameInFlightIndex] };
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
	if (m_graphics->m_queueGraphics.submit(1, &submitInfo, m_graphics->m_frameInFlightFences[m_graphics->m_frameInFlightIndex]) != vk::Result::eSuccess)
		throw std::runtime_error("failed to submit draw command buffer!");

	vk::PresentInfoKHR presentInfo{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores.data(),
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

	m_graphics->SetupVulkan();
	m_textureManager->CreateTextures();
	m_modelManger->CreateModels();

	m_graphics->CreateDynamicUniformBuffers();

	m_graphics->CreateDescriptorPool(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSetLayout(m_textureManager->GetNumberOfTextures());
	m_graphics->CreateDescriptorSets(m_textureManager->m_textureDescriptors);

	m_graphics->CreateGraphicsPipeline();
	m_graphics->CreateDepthResources();
	m_graphics->CreateFrameBuffers();
	m_graphics->CreateSyncObjects();

	LOG_SUCCESS("Graphics initialized successfully!");
}

Renderer::~Renderer()
{
}
