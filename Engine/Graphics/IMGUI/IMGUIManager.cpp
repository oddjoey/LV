#include "IMGUIManager.h"

CLASS_SINGLETON_SOURCE(IMGUIManager)

void IMGUIManager::CheckVKResult(VkResult err)
{
	if (err == VK_SUCCESS)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

IMGUIManager::IMGUIManager()
{
	m_graphics = Graphics::Get();
	m_cameraManager = CameraManager::Get();
	m_objectManager = ObjectManager::Get();
	m_modelManager = ModelManager::Get();
	m_textureManager = TextureManager::Get();
	m_performanceManager = PerformanceManager::Get();

	CreateDescriptorPool();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableSetMousePos;
	io.DisplaySize = ImVec2 { static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.width),
		static_cast<float>(m_graphics->m_swapChainInfo.imageExtent.height) };

	ImGui::StyleColorsDark();

	if (!ImGui_ImplGlfw_InitForVulkan(m_graphics->m_window, true))
		throw std::runtime_error("ImGui_ImplGlfw_InitForVulkan failed!");

	ImGui_ImplVulkan_PipelineInfo pipelineInfoMain = {
		.RenderPass = m_graphics->m_renderPass,
		.Subpass = 1,
		.MSAASamples = static_cast<VkSampleCountFlagBits>(m_graphics->m_MSAALevel),
		.ExtraDynamicStates = {},
		.PipelineRenderingCreateInfo = {}
	};

	ImGui_ImplVulkan_InitInfo initInfo = {
		.ApiVersion = m_graphics->applicationInfo.apiVersion,
		.Instance = m_graphics->m_vulkanInstance,
		.PhysicalDevice = m_graphics->m_physicalDevice,
		.Device = m_graphics->m_logicalDevice,
		.QueueFamily = m_graphics->m_physicalDeviceGraphicsIndex,
		.Queue = m_graphics->m_queueGraphics,
		.DescriptorPool = m_descriptorPool,
		.DescriptorPoolSize = 0,
		.MinImageCount = m_graphics->m_windowSurfaceCapabilities.minImageCount,
		.ImageCount = m_graphics->m_swapChainImageCount,
		.PipelineCache = 0,
		.PipelineInfoMain = pipelineInfoMain,
		.UseDynamicRendering = false,
		.Allocator = 0,
		.CheckVkResultFn = CheckVKResult,
		.MinAllocationSize = 0 // 1024*1024 if using, see struct definition
	};

	ImGui_ImplVulkan_Init(&initInfo);
}

IMGUIManager::~IMGUIManager()
{

}

void IMGUIManager::CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eSampler, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
		// ImGui 1.92's Vulkan backend binds textures as separate sampled images and samplers.
		{ vk::DescriptorType::eSampledImage, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE }
		//{ vk::DescriptorType::eSampledImage, 1000 },
		//{ vk::DescriptorType::eStorageImage, 1000 },
		//{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		//{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		//{ vk::DescriptorType::eUniformBuffer, 1000 },
		//{ vk::DescriptorType::eStorageBuffer, 1000 },
		//{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		//{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		//{ vk::DescriptorType::eInputAttachment, 1000 }
	};
	
	vk::DescriptorPoolCreateInfo poolCreateInfo = {
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = static_cast<uint32_t>(IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE),
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	if (m_graphics->m_logicalDevice.createDescriptorPool(&poolCreateInfo, 0, &m_descriptorPool) != vk::Result::eSuccess)
		throw std::runtime_error("Failed to create IMGUI descriptor pool!");
}

void IMGUIManager::WindowSetup()
{
	if (ImGui::Begin("Lordi"))
	{
		if (ImGui::BeginChild("Menu"))
			DrawDebugContents();
		ImGui::EndChild();
	}
	ImGui::End();
}

void IMGUIManager::SetDebugWindowEnabled(bool enabled)
{
	m_debugWindowEnabled = enabled;
	if (!enabled)
		m_showIMGUI = false;
}

void IMGUIManager::DrawDebugContents(bool showPerformance)
{
	{
		{
			if (showPerformance && ImGui::CollapsingHeader("Performance"))
			{
				ImGui::Text("FPS: %f", m_performanceManager->GetFPS());
				ImGui::Text("Graphics Command Buffer Duration: %fms", m_performanceManager->GetGraphicsCommandBufferDuration().count());
				ImGui::Text("IMGUI Command Buffer Duration: %fms", m_performanceManager->GetIMGUICommandBufferDuration().count());
				auto cpuDur = m_performanceManager->GetCPUWaitForGPUDuration();
				ImGui::Text("CPU Wait For GPU: %fms", m_performanceManager->GetCPUWaitForGPUDuration().count());
			}
			if (ImGui::CollapsingHeader("World"))
			{
				if (ImGui::TreeNode("Cameras"))
				{
					ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.3f, 0.6f, 0.6f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.3f, 0.7f, 0.7f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.3f, 0.8f, 0.8f));
					if (ImGui::Button("Add Camera"))
						m_cameraManager->CreateCamera({});
					ImGui::PopStyleColor(3);

					auto cameras = m_cameraManager->GetCameras();
					for (auto& camera : cameras)
					{
						std::string cameraName = "Camera " + std::to_string(camera->GetIndex());
						if (ImGui::TreeNode(cameraName.c_str()))
						{
							bool isActive = m_cameraManager->GetActiveCamera() == camera;
							if (ImGui::Checkbox("Active", &isActive))
								m_cameraManager->SetActiveCamera(isActive ? camera : nullptr);

							ImGui::DragFloat3("Position", RAW_DATA(camera->m_position), 0.1f);
							
							auto eulerAngles = camera->GetAngles();
							ImGui::DragFloat3("Rotation", RAW_DATA(eulerAngles), 0.1f);
							camera->SetAngle(eulerAngles);

							ImGui::InputFloat("FOV", &camera->m_fov, 0.1f);
							ImGui::InputFloat("Near Plane", &camera->m_nearPlane, 0.1f);
							ImGui::InputFloat("Far Plane", &camera->m_farPlane, 0.1f);

							ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.f, 0.6f, 0.6f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.f, 0.7f, 0.7f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.f, 0.8f, 0.8f));
							if (ImGui::Button("Delete Camera"))
								m_cameraManager->RemoveCamera(camera);
							ImGui::PopStyleColor(3);
							
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Objects"))
				{
					ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.3f, 0.6f, 0.6f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.3f, 0.7f, 0.7f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.3f, 0.8f, 0.8f));
					if (ImGui::Button("Add Object"))
						m_objectManager->CreateObject({});
					ImGui::PopStyleColor(3);

					auto objects = m_objectManager->GetObjects();
					auto modelNames = m_modelManager->GetModelNames();
					auto textureNames = m_textureManager->GetTextureNames();

					for (auto& object : objects)
					{
						std::string objectName = "Object " + std::to_string(object->GetIndex());
						if (ImGui::TreeNode(objectName.c_str()))
						{
							auto pos = object->GetPosition();
							ImGui::DragFloat3("Position", RAW_DATA(pos), 0.1f);
							object->SetPosition(pos);

							auto eulerAngles = object->GetAngles();
							ImGui::DragFloat3("Rotation", RAW_DATA(eulerAngles), 0.1f);
							object->SetAngle(eulerAngles);

							auto scale = object->GetScale();
							ImGui::DragFloat3("Scale", RAW_DATA(scale), 0.1f);
							object->SetScale(scale);

							auto modelIndex = object->GetModelIndex();
							const char* previewModelName = modelNames[modelIndex].c_str();
							if (ImGui::BeginCombo("Model", previewModelName))
							{
								static ImGuiTextFilter filter;
								if (ImGui::IsWindowAppearing())
								{
									ImGui::SetKeyboardFocusHere();
									filter.Clear();
								}
								ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
								filter.Draw("##Filter", -FLT_MIN);

								for (uint32_t i = 0; i < modelNames.size(); i++)
									if (filter.PassFilter(modelNames[i].c_str()))
									{
										auto modelName = modelNames[i].c_str();
										if (ImGui::Selectable(modelNames[i].c_str(), modelIndex == i))
											modelIndex = i;
									}

								ImGui::EndCombo();
							}
							m_objectManager->ChangeObjectModel(object, modelIndex);

							auto textureIndex = object->GetTextureIndex();
							const char* previewTextureName = textureNames[textureIndex].c_str();
							if (ImGui::BeginCombo("Texture", previewTextureName))
							{
								static ImGuiTextFilter filter;
								if (ImGui::IsWindowAppearing())
								{
									ImGui::SetKeyboardFocusHere();
									filter.Clear();
								}
								ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
								filter.Draw("##Filter", -FLT_MIN);

								for (uint32_t i = 0; i < textureNames.size(); i++)
									if (filter.PassFilter(textureNames[i].c_str()))
										if (ImGui::Selectable(textureNames[i].c_str(), textureIndex == i))
											textureIndex = i;

								ImGui::EndCombo();
							}
							object->SetTexture(textureIndex);

							ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.f, 0.6f, 0.6f));
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.f, 0.7f, 0.7f));
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.f, 0.8f, 0.8f));
							if (ImGui::Button("Delete Object"))
							{
								m_objectManager->RemoveObject(object);

							}
							ImGui::PopStyleColor(3);

							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
			}
		}
	}
}

void IMGUIManager::SetGUICallback(const std::function<void()>& callback)
{
	m_guiCallback = callback;
}

void IMGUIManager::RecordCommandBuffer(const vk::CommandBuffer& commandBuffer)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (m_showIMGUI && m_debugWindowEnabled)
		WindowSetup();
	if (m_guiCallback)
		m_guiCallback();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void IMGUIManager::Cleanup()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
		
	m_graphics->m_logicalDevice.destroyDescriptorPool(m_descriptorPool, nullptr);
}