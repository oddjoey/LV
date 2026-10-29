#pragma once

#include "../Graphics.h"

#include "Model.h"

class ModelManager
{
	friend class Renderer;
	CLASS_SINGLETON_HEADER(ModelManager)
public:
	ModelManager();
	~ModelManager();
	void AddModel(const pModel& model);
	pModel CreateModel(const Model& modelInfo);
	void CreateModels();
	uint32_t GetModelIndex(const std::string& modelName);
	uint32_t GetModelCount() const;
	// index as returned by Model::GetIndex (1-based); nullptr if out of range.
	pModel GetModel(const uint32_t& index) const;
	// The same without a reference count, for loops over every object: valid while the model is loaded.
	const Model* PeekModel(uint32_t index) const;
	std::vector<std::string> GetModelNames() const;
private:

	std::vector<pModel>						m_models = {};

	vk::Buffer								m_vertexBuffer;
	std::vector<vk::DeviceSize>				m_verticesOffsets = {};

	vk::Buffer								m_indexBuffer;
	std::vector<uint32_t>					m_indicesOffsets = {};
	std::vector<uint32_t>					m_indicesCounts = {};
};