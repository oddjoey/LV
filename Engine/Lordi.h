#pragma once

#ifdef _DEBUG

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#endif // _DEBUG

#include "Settings.h"
#include "Graphics/Graphics.h"
#include "Graphics/Objects/ObjectManager.h"
#include "Graphics/Renderer.h"

class Lordi
{
	static std::shared_ptr<Lordi> _instance;
	Lordi();
public:
	~Lordi();

	Lordi(const Lordi&) = delete;
	Lordi& operator=(const Lordi& r) = delete;

	static std::shared_ptr<Lordi> Get();

	std::shared_ptr<Graphics> GetGraphics() const;
	std::shared_ptr<Settings> GetSettings() const;

	void Initialize();
private:
	// Initialize engine systems
	bool InitializeSystems();

	void MainLoop();
};

