#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Model::Model(const std::string& modelName, const std::string& modelPath) : m_name(modelName), m_path(modelPath)
{
	if (modelName.empty() || modelPath.empty())
		throw std::runtime_error("Model parameters not valid!");

	tinyobj::attrib_t at;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&at, &shapes, &materials, &err, m_path.c_str()))
		throw std::runtime_error(err);

	glm::vec3 minBounds(FLT_MAX);
	glm::vec3 maxBounds(-FLT_MAX);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto& shape : shapes)
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};

			vertex.position = {
				at.vertices[3 * index.vertex_index],
				at.vertices[3 * index.vertex_index + 1],
				at.vertices[3 * index.vertex_index + 2]
			};

			minBounds = glm::min(minBounds, vertex.position);
			maxBounds = glm::max(maxBounds, vertex.position);

			vertex.texCoord = {
				at.texcoords[2 * index.texcoord_index],
				1.f - at.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.f, 1.f, 1.f };

			if (!uniqueVertices.contains(vertex))
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
				m_vertices.push_back(vertex);
			}

			m_indices.push_back(uniqueVertices[vertex]);
		}

	glm::vec3 center = (minBounds + maxBounds) * 0.5f;
	glm::vec3 size = maxBounds - minBounds;

	float maxExtent = std::max({size.x, size.y, size.z});
	float scale = 2.f / maxExtent;

	for (auto& vertex : m_vertices)
		vertex.position = (vertex.position - center) * scale;
}

Model::~Model()
{
}

const std::vector<Vertex>& Model::GetVertices() const
{
	return m_vertices;
}

const std::vector<uint32_t>& Model::GetIndices() const
{
	return m_indices;
}

const uint32_t Model::GetIndex() const
{
	return m_index;
}

void Model::SetIndex(const uint32_t& index)
{
	m_index = index;
}