#pragma once

#include "../Graphics.h"

#include "Model.h"

class ModelManager
{
public:
	static std::shared_ptr<ModelManager> Get();
	void AddModel(Model model);
	void CreateModels();
	uint8_t GetModelIndex(const std::string& modelName);
	ModelManager();
	~ModelManager();
private:
	friend class Renderer;

	static std::shared_ptr<ModelManager>	s_instance;

	std::vector<Model>						m_textures = {};

	vk::Buffer								m_vertexBuffer;
	std::vector<vk::DeviceSize>				m_vertexOffsets = {};

	vk::Buffer								m_indexBuffer;
	std::vector<uint32_t>					m_indexOffsets = {};
	std::vector<uint32_t>					m_indexSizes = {};
};