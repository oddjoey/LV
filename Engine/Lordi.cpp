#include "Lordi.h"

std::shared_ptr<Lordi> Lordi::_instance = nullptr;

std::shared_ptr<Lordi> Lordi::Get()
{
	if (_instance == nullptr)
		_instance = std::shared_ptr<Lordi>(new Lordi());

	return _instance;
}

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
	auto sheepTexture = m_textureManager->CreateTexture({ "Sheep", "Engine/Graphics/Resources/sheep.png" });
	auto bearTexture = m_textureManager->CreateTexture({ "Bear", "Engine/Graphics/Resources/BearMacaroon.png" });

	m_modelManger = ModelManager::Get();
	auto sheepModel = m_modelManger->CreateModel({ "Sheep", "Engine/Graphics/Resources/sheep.obj" });
	auto bearModel = m_modelManger->CreateModel({ "Bear", "Engine/Graphics/Resources/BearMacaroon.obj" });

	m_objectManager = ObjectManager::Get();
	auto sheep = m_objectManager->CreateObject({ { 0.f, 0.f, 0.f }, { 90.f, 90.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, sheepModel->GetIndex(), sheepTexture->GetIndex() });
	auto bear = m_objectManager->CreateObject({ { 0.f, -1.f, 0.f }, { 90.f, 90.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, bearModel->GetIndex(), bearTexture->GetIndex() });

	m_cameraManager = CameraManager::Get();
	auto camera = m_cameraManager->CreateCamera({ { -2.f, -2.f, 2.f }, { 0.f, 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, 45.f, 0.1f, 10.f });
	camera->LookAt({ 0.f, 0.f, 0.f });
	m_cameraManager->SetActiveCamera(camera);

	m_settings = Settings::Get();
	m_renderer = Renderer::Get();
	m_imGUIManager = IMGUIManager::Get();

	LOG_SUCCESS("Lordi's systems initialized successfully!");
	 
	return true;
}

void Lordi::MainLoop()
{
	while (!glfwWindowShouldClose(m_graphics->m_window))
	{
		glfwPollEvents();
		m_renderer->RenderFrame();
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

void Lordi::Initialize()
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
