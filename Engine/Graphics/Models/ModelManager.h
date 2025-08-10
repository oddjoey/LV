#pragma once

#include "../Graphics.h"

#include "Model.h"

class ModelManager
{
public:
	static std::shared_ptr<ModelManager> Get();
	void AddModel(const pModel& model);
	pModel CreateModel(const Model& modelInfo);
	void CreateModels();
	uint32_t GetModelIndex(const std::string& modelName);
	uint32_t GetModelCount() const;
	ModelManager();
	~ModelManager();
private:
	friend class Renderer;

	static std::shared_ptr<ModelManager>	s_instance;

	std::vector<pModel>						m_models = {};

	vk::Buffer								m_vertexBuffer;
	std::vector<vk::DeviceSize>				m_verticesOffsets = {};

	vk::Buffer								m_indexBuffer;
	std::vector<uint32_t>					m_indicesOffsets = {};
	std::vector<uint32_t>					m_indicesCounts = {};
};