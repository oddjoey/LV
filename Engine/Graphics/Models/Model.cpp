#include "Model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

Model::Model(const std::string& modelName, const std::string& modelPath, const std::string& texturePath) : m_name(modelName)
{
	tinyobj::attrib_t at;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&at, &shapes, &materials, &err, modelPath.c_str()))
		throw std::runtime_error(err);

	for (const auto& shape : shapes)
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex = {};

			vertex.position = {
				at.vertices[3 * index.vertex_index],
				at.vertices[3 * index.vertex_index + 1],
				at.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				at.texcoords[2 * index.texcoord_index],
				at.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.f, 1.f, 1.f };

			m_vertices.push_back(vertex);
			m_indices.push_back(static_cast<uint32_t>(m_indices.size()));
		}
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

const uint8_t Model::GetIndex() const
{
	return m_index;
}

void Model::SetIndex(const uint8_t& index)
{
	m_index = index;
}
