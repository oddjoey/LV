#pragma once

#include "Graphics.h"
#include "Objects/ObjectManager.h"
#include "Models/TextureManager.h"
#include "Models/ModelManager.h"

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
	const int							m_maxFramesInFlight = 2;
	uint32_t							m_frameInFlightIndex = 0;
};
