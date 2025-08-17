#include "ModelManager.h"

CLASS_SINGLETON_SOURCE(ModelManager)

ModelManager::ModelManager()
{

}

ModelManager::~ModelManager()
{

}

void ModelManager::AddModel(const pModel& model)
{
	m_models.push_back(model);
	model->SetIndex(static_cast<uint8_t>(m_models.size()));
}

pModel ModelManager::CreateModel(const Model& modelInfo)
{
	auto model = std::make_shared<Model>(modelInfo);
	AddModel(model);
	return model;
}

void ModelManager::CreateModels()
{
	LOG_INFO("Creating models...");
	auto graphics = Graphics::Get();

	std::vector<Vertex> vertexData = {};
	std::vector<uint32_t> indexData = {};
	for (auto model : m_models)
	{
		auto vertices = model->GetVertices();

		m_verticesOffsets.push_back(static_cast<uint32_t>(vertexData.size()));

		vertexData.insert(vertexData.end(), vertices.begin(), vertices.end());

		auto indices = model->GetIndices();

		m_indicesOffsets.push_back(static_cast<uint32_t>(indexData.size()));

		indexData.insert(indexData.end(), indices.begin(), indices.end());

		m_indicesCounts.push_back(static_cast<uint32_t>(indices.size()));
	}

	vk::DeviceMemory vertexMemory, indexMemory;
	m_vertexBuffer = graphics->CreateBuffer(vertexData.size() * sizeof(Vertex), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, vertexMemory, true);
	graphics->CopyDataToBuffer(vertexData.data(), static_cast<vk::DeviceSize>(vertexData.size() * sizeof(Vertex)), m_vertexBuffer);

	m_indexBuffer = graphics->CreateBuffer(indexData.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, indexMemory, true);
	graphics->CopyDataToBuffer(indexData.data(), static_cast<vk::DeviceSize>(indexData.size() * sizeof(uint32_t)), m_indexBuffer);

	LOG_SUCCESS("Sucessfully created models!");
}

uint32_t ModelManager::GetModelIndex(const std::string& modelName)
{
	for (const auto& model : m_models)
		if (!model->m_name.compare(modelName))
			return model->GetIndex();

	throw std::runtime_error("Model not found!");
	return 0;
}

uint32_t ModelManager::GetModelCount() const
{
	return static_cast<uint32_t>(m_models.size());
}

std::vector<std::string> ModelManager::GetModelNames() const
{
	std::vector<std::string> names(m_models.size() + 1);
	names[0] = "None";
	for (uint32_t i = 0; i < m_models.size(); i++)
		names[i + 1] = m_models[i]->m_name;
	return names;
}