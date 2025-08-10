#pragma once

#include "Graphics.h"
#include "Objects/ObjectManager.h"
#include "Models/TextureManager.h"
#include "Models/ModelManager.h"
#include "Objects/CameraManager.h"

class Renderer
{
	static std::shared_ptr<Renderer> _instance;
public:
	static std::shared_ptr<Renderer> Get();

	void RenderFrame();
	Renderer();
	~Renderer();
private:
	std::shared_ptr<Graphics>			m_graphics = nullptr;
	std::shared_ptr<ModelManager>		m_modelManger = nullptr;
	std::shared_ptr<TextureManager>		m_textureManager = nullptr;
	std::shared_ptr<ObjectManager>		m_objectManager = nullptr;
	std::shared_ptr<CameraManager>		m_cameraManager = nullptr;
};
