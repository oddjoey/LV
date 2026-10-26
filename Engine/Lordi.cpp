#include "Lordi.h"

#include <thread>

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
	m_modelManger = ModelManager::Get();
	m_objectManager = ObjectManager::Get();
	m_cameraManager = CameraManager::Get();

	// Assets have to be registered before the renderer uploads them.
	if (m_game)
		m_game->OnLoadAssets();

	m_settings = Settings::Get();
	m_renderer = Renderer::Get();
	m_imGUIManager = IMGUIManager::Get();
	if (m_game)
		m_imGUIManager->SetGUICallback([game = m_game] { game->OnGUI(); });
	m_inputManager = InputManager::Get();
	m_performanceManager = PerformanceManager::Get();

	LOG_SUCCESS("Lordi's systems initialized successfully!");
	 
	return true;
}

void Lordi::MainLoop()
{
	auto lastFrameTime = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(m_graphics->m_window))
	{
		glfwPollEvents();
		m_inputManager->Update();

		auto frameTime = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(frameTime - lastFrameTime).count();
		lastFrameTime = frameTime;
		if (m_game)
			m_game->OnUpdate(deltaTime);

		m_renderer->RenderFrame();
		m_performanceManager->Tick();

		if (m_frameLimit > 0)
			std::this_thread::sleep_until(frameTime + std::chrono::microseconds(1000000 / m_frameLimit));
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

void Lordi::SetGame(const std::shared_ptr<Game>& game)
{
	m_game = game;
}

void Lordi::SetFrameLimit(int framesPerSecond)
{
	m_frameLimit = (std::max)(framesPerSecond, 0);
}

void Lordi::RequestQuit()
{
	if (m_graphics)
		glfwSetWindowShouldClose(m_graphics->m_window, GLFW_TRUE);
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

		if (m_game)
			m_game->OnStart();

		MainLoop();

		if (m_game)
			m_game->OnShutdown();

		m_imGUIManager->Cleanup();
		m_graphics->CleanupVulkan();
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(std::string(e.what()));
		return;
	}
}
