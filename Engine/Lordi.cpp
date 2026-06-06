#include "Lordi.h"

CLASS_SINGLETON_SOURCE(Lordi)

Lordi::Lordi()
{    

}

Lordi::~Lordi()
{

}

bool Lordi::InitializeSystems()
{
	LOG_INFO("Initializing Lordi's systems...");
	m_graphics = Graphics::Get();

	m_textureManager = TextureManager::Get();
	auto ak47Texture = m_textureManager->CreateTexture({ "AK47", "Engine/Graphics/Resources/ak47.jpg" });
	//auto bearTexture = m_textureManager->CreateTexture({ "Bear", "Engine/Graphics/Resources/BearMacaroon.png" });

	m_modelManger = ModelManager::Get();
	auto ak47Model = m_modelManger->CreateModel({ "AK47", "Engine/Graphics/Resources/ak47.obj" });
	//auto bearModel = m_modelManger->CreateModel({ "Bear", "Engine/Graphics/Resources/BearMacaroon.obj" });

	m_objectManager = ObjectManager::Get();
	auto ak47 = m_objectManager->CreateObject({ { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, ak47Model->GetIndex(), ak47Texture->GetIndex() });
	//auto bear = m_objectManager->CreateObject({ { 0.f, -1.f, 0.f }, { 0.f, 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, bearModel->GetIndex(), bearTexture->GetIndex() });

	m_cameraManager = CameraManager::Get();

	Camera cameraInfo = {};
	cameraInfo.SetPosition({ 2.f, 2.f, 3.f })->SetAngle({ 0.f, 0.f, 0.f })->SetScale({ 1.f, 1.f, 1.f });
	cameraInfo.SetFov(45.f)->SetNearPlane(0.1f)->SetFarPlane(10.f)->LookAt({ 0.f, 0.f, 0.f });
	auto camera = m_cameraManager->CreateCamera(cameraInfo);
	m_cameraManager->SetActiveCamera(camera);

	m_settings = Settings::Get();
	m_renderer = Renderer::Get();
	m_imGUIManager = IMGUIManager::Get();
	m_inputManager = InputManager::Get();
	m_performanceManager = PerformanceManager::Get();

	LOG_SUCCESS("Lordi's systems initialized successfully!");
	 
	return true;
}

void Lordi::MainLoop()
{
	while (!glfwWindowShouldClose(m_graphics->m_window))
	{
		glfwPollEvents();
		m_renderer->RenderFrame();
		m_performanceManager->Tick();
	}

	vkDeviceWaitIdle(m_graphics->m_logicalDevice);
}

std::shared_ptr<Graphics> Lordi::GetGraphics() const
{
	return m_graphics;
}

std::shared_ptr<Settings> Lordi::GetSettings() const
{
	return Settings::Get();
}

void Lordi::Start()
{
	LOG_INFO("Starting Lordi...");

#ifdef _DEBUG
	// memory leak detection
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	LOG_DEBUG("Memory leak detection enabled!");
#endif // _DEBUG

	try
	{
		if (!InitializeSystems())
			throw std::runtime_error("Failed to start Lordi.");

		MainLoop();

		m_imGUIManager->Cleanup();
		m_graphics->CleanupVulkan();
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(std::string(e.what()));
		return;
	}
}
