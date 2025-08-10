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

	std::shared_ptr<Renderer>			m_renderer = nullptr;
	std::shared_ptr<Graphics>			m_graphics = nullptr;
	std::shared_ptr<Settings>			m_settings = nullptr;
	std::shared_ptr<ModelManager>		m_modelManger = nullptr;
	std::shared_ptr<TextureManager>		m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>		m_objectManager = nullptr;
	std::shared_ptr<CameraManager>		m_cameraManager = nullptr;
};

