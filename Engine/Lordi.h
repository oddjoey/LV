#pragma once

#ifdef _DEBUG

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#endif // _DEBUG

#include "Utility/Settings.h"
#include "Graphics/Renderer.h"
#include "Objects/ObjectManager.h"
#include "Utility/InputManager.h"
#include "Utility/PerformanceManager.h"
#include "Game.h"

class Lordi
{
	CLASS_SINGLETON_HEADER(Lordi)
public:
	~Lordi();

	Lordi(const Lordi&) = delete;
	Lordi& operator=(const Lordi& r) = delete;

	std::shared_ptr<Graphics> GetGraphics() const;
	std::shared_ptr<Settings> GetSettings() const;

	// Must be called before Start.
	void SetGame(const std::shared_ptr<Game>& game);

	// Ends the main loop after the current frame and shuts down normally.
	void RequestQuit();

	// Caps the frame rate by sleeping out the rest of each frame. 0 = no cap.
	void SetFrameLimit(int framesPerSecond);

	void Start();
private:
	Lordi();
	// Initialize engine systems
	bool InitializeSystems();

	void MainLoop();

	std::shared_ptr<Renderer>				m_renderer = nullptr;
	std::shared_ptr<Graphics>				m_graphics = nullptr;
	std::shared_ptr<Settings>				m_settings = nullptr;
	std::shared_ptr<ModelManager>			m_modelManger = nullptr;
	std::shared_ptr<TextureManager>			m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>			m_objectManager = nullptr;
	std::shared_ptr<CameraManager>			m_cameraManager = nullptr;
	std::shared_ptr<IMGUIManager>			m_imGUIManager = nullptr;
	std::shared_ptr<InputManager>			m_inputManager = nullptr;
	std::shared_ptr<PerformanceManager>		m_performanceManager = nullptr;
	std::shared_ptr<Game>					m_game = nullptr;
	int										m_frameLimit = 0;
};

