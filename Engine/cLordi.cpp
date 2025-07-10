#include "cLordi.h"

std::shared_ptr<cLordi> cLordi::_instance = nullptr;

std::shared_ptr<cLordi> cLordi::Get()
{
	if (_instance == nullptr)
		_instance = std::make_shared<cLordi>();

	return _instance;
}

cLordi::cLordi()
{
	if (!InitializeSystems())
	{
		std::cerr << "Failed to start Lordi.\n";
		return;
	}

	MainLoop();
}

cLordi::~cLordi()
{
}

bool cLordi::InitializeSystems()
{
	try
	{
		cGraphics::Get();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return false;
	}
	 
	return true;
}

void cLordi::MainLoop()
{
	while (!glfwWindowShouldClose(GetGraphics()->m_pWindow))
	{
		glfwPollEvents();
	}
}

std::shared_ptr<cGraphics> cLordi::GetGraphics() const
{
	return cGraphics::Get();
}
