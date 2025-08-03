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
	Graphics::Get();

	Model model = { "Engine/Graphics/Resources/viking_room.obj", "" };

	ModelManager::Get()->AddModel(model);
	ModelManager::Get()->InitializeModelsToMemory();

	ObjectManager::Get();
	Renderer::Get();

	LOG_SUCCESS("Lordi's systems initialized successfully!");
	 
	return true;
}

void Lordi::MainLoop()
{
	auto renderer = Renderer::Get();

	while (!glfwWindowShouldClose(GetGraphics()->m_window))
	{
		glfwPollEvents();
		renderer->RenderFrame();
	}

	vkDeviceWaitIdle(GetGraphics()->m_logicalDevice);
}

std::shared_ptr<Graphics> Lordi::GetGraphics() const
{
	return Graphics::Get();
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
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(std::string(e.what()));
		return;
	}
}
