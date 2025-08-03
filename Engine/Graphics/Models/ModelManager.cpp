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
	model.SetIndex(static_cast<uint8_t>(m_models.size()));
	m_models.push_back(model);
}

void ModelManager::InitializeModelsToMemory()
{
	auto graphics = Graphics::Get();

	vk::DeviceSize vertexSize = {};
	std::vector<Vertex> vertexData = {};
	std::vector<uint32_t> indexData = {};
	for (auto& model : m_models)
	{
		auto vertices = model.GetVertices();

		m_vertexOffsets.push_back(vertexSize);

		vertexSize += vertices.size();
		vertexData.insert(vertexData.end(), vertices.begin(), vertices.end());

		for (auto& index : model.GetIndices())
			indexData.push_back(static_cast<uint32_t>(index + vertexSize));
	}

	m_vertexBuffer = graphics->CreateBuffer(vertexSize * sizeof(Vertex), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, m_vertexBufferMemory);
	graphics->MoveDataToDeviceBuffer(vertexData.data(), vertexSize * sizeof(Vertex), m_vertexBuffer);

	m_indexBuffer = graphics->CreateBuffer(indexData.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal, m_indexBufferMemory);
	graphics->MoveDataToDeviceBuffer(indexData.data(), static_cast<vk::DeviceSize>(indexData.size() * sizeof(uint32_t)), m_indexBuffer);

	m_indicesSize = static_cast<uint32_t>(indexData.size());
}

void ModelManager::Cleanup()
{
	auto graphics = Graphics::Get();

	graphics->m_logicalDevice.destroyBuffer(m_indexBuffer, nullptr);
	graphics->m_logicalDevice.freeMemory(m_indexBufferMemory, nullptr);

	graphics->m_logicalDevice.destroyBuffer(m_vertexBuffer, nullptr);
	graphics->m_logicalDevice.freeMemory(m_vertexBufferMemory, nullptr);
}

uint8_t ModelManager::GetModelIndex(const std::string& modelName)
{
	for (const Model& model : m_models)
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
	Cleanup();
}