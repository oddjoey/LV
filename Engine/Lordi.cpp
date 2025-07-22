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

	try
	{
		Graphics::Get()->Initialize();
	}
	catch (const std::exception& e)
	{
		LOG_FATAL(std::string(e.what()));
		return false;
	}

	LOG_SUCCESS("Lordi's systems initialized successfully!");
	 
	return true;
}

void Lordi::MainLoop()
{
	while (!glfwWindowShouldClose(GetGraphics()->m_pWindow))
	{
		glfwPollEvents();
		GetGraphics()->DrawFrame();
	}

	vkDeviceWaitIdle(GetGraphics()->m_pLogicalDevice);
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
	LOG_DEBUG("Memory leak dectection enabled!");
#endif // _DEBUG

	if (!InitializeSystems())
	{
		LOG_FATAL("Failed to start Lordi.");
		exit(EXIT_FAILURE);
	}

	MainLoop();
}
