#include "ModelManager.h"

std::shared_ptr<ModelManager> ModelManager::s_instance = nullptr;

std::shared_ptr<ModelManager> ModelManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<ModelManager>(new ModelManager());

	return s_instance;
}

void ModelManager::AddModel(Model model)
{
	model.SetIndex(static_cast<uint8_t>(m_textures.size()));
	m_textures.push_back(model);
}

void ModelManager::CreateModels()
{
	LOG_INFO("Creating models...");
	auto graphics = Graphics::Get();

	std::vector<Vertex> vertexData = {};
	std::vector<uint32_t> indexData = {};
	for (auto& model : m_textures)
	{
		auto vertices = model.GetVertices();

		m_vertexOffsets.push_back(static_cast<uint32_t>(vertexData.size()));

		vertexData.insert(vertexData.end(), vertices.begin(), vertices.end());

		auto indices = model.GetIndices();

		m_indexOffsets.push_back(static_cast<uint32_t>(indexData.size()));

		indexData.insert(indexData.end(), indices.begin(), indices.end());

		m_indexSizes.push_back(static_cast<uint32_t>(indexData.size()));
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

uint8_t ModelManager::GetModelIndex(const std::string& modelName)
{
	for (const Model& model : m_textures)
		if (!model.m_name.compare(modelName))
			return model.GetIndex();

	throw std::runtime_error("Model not found!");
	return 0;
}

ModelManager::ModelManager()
{

}

ModelManager::~ModelManager()
{

}