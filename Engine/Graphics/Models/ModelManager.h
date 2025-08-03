#pragma once

#include "../Graphics.h"

#include "Model.h"

class ModelManager
{
public:
	static std::shared_ptr<ModelManager> Get();
	void AddModel(Model model);
	void InitializeModelsToMemory();
	void Cleanup();
	uint8_t GetModelIndex(const std::string& modelName);
	ModelManager();
	~ModelManager();
private:
	friend class Renderer;

	static std::shared_ptr<ModelManager> s_instance;

	std::vector<Model>			m_models = {};

	vk::Buffer					m_vertexBuffer;
	vk::DeviceMemory			m_vertexBufferMemory;
	std::vector<vk::DeviceSize>	m_vertexOffsets = {};

	vk::Buffer					m_indexBuffer;
	vk::DeviceMemory			m_indexBufferMemory;
	uint32_t					m_indicesSize = 0;
};