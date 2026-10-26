#include "Model.h"

#include <algorithm>
#include <stdexcept>

#include <glm/gtc/quaternion.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#include <tiny_obj_loader.h>
#include <ufbx.h>

Model::Model(const std::string& modelName, const std::string& modelPath)
	: Model(modelName, modelPath, ModelImportOptions{ .normalize = true })
{
}

Model::Model(const std::string& modelName, const std::string& modelPath, const ModelImportOptions& options)
	: m_name(modelName), m_path(modelPath)
{
	if (modelName.empty() || modelPath.empty())
		throw std::runtime_error("Model parameters not valid!");

	std::string extension = modelPath.substr(modelPath.find_last_of('.') + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (extension == "fbx")
		LoadFbx();
	else
		LoadObj();

	if (m_vertices.empty())
		throw std::runtime_error("Model has no geometry: " + modelPath);

	ApplyImportOptions(options);
	CalculateBounds();
}

void Model::AddVertex(const Vertex& vertex, std::unordered_map<Vertex, uint32_t>& uniqueVertices)
{
	auto it = uniqueVertices.find(vertex);
	if (it == uniqueVertices.end())
	{
		it = uniqueVertices.emplace(vertex, static_cast<uint32_t>(m_vertices.size())).first;
		m_vertices.push_back(vertex);
	}
	m_indices.push_back(it->second);
}

void Model::LoadObj()
{
	tinyobj::attrib_t at;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string warn;

	if (!tinyobj::LoadObj(&at, &shapes, &materials, &err, &warn, m_path.c_str()))
		throw std::runtime_error(err);

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto& shape : shapes)
	{
		// tinyobj triangulates, so indices come in threes.
		const auto& indices = shape.mesh.indices;
		for (size_t first = 0; first + 2 < indices.size(); first += 3)
		{
			Vertex triangle[3] = {};
			for (int corner = 0; corner < 3; corner++)
			{
				const auto& index = indices[first + corner];
				Vertex& vertex = triangle[corner];
				vertex.position = {
					at.vertices[3 * index.vertex_index],
					at.vertices[3 * index.vertex_index + 1],
					at.vertices[3 * index.vertex_index + 2]
				};
				if (index.texcoord_index >= 0)
					vertex.texCoord = { at.texcoords[2 * index.texcoord_index], 1.f - at.texcoords[2 * index.texcoord_index + 1] };
				vertex.color = { 1.f, 1.f, 1.f };
				if (index.normal_index >= 0)
					vertex.normal = { at.normals[3 * index.normal_index], at.normals[3 * index.normal_index + 1], at.normals[3 * index.normal_index + 2] };
			}

			// Files without normals get flat face normals so they still light properly.
			if (indices[first].normal_index < 0)
			{
				glm::vec3 faceNormal = glm::cross(triangle[1].position - triangle[0].position, triangle[2].position - triangle[0].position);
				faceNormal = glm::length(faceNormal) > 0.f ? glm::normalize(faceNormal) : glm::vec3(0.f, 1.f, 0.f);
				for (auto& vertex : triangle)
					vertex.normal = faceNormal;
			}

			for (const auto& vertex : triangle)
				AddVertex(vertex, uniqueVertices);
		}
	}
}

void Model::LoadFbx()
{
	ufbx_load_opts options = {};
	options.target_axes = ufbx_axes_right_handed_y_up;
	options.target_unit_meters = 1.f;

	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(m_path.c_str(), &options, &error);
	if (!scene)
		throw std::runtime_error("Failed to load FBX " + m_path + ": " + std::string(error.description.data, error.description.length));

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (size_t nodeIndex = 0; nodeIndex < scene->nodes.count; nodeIndex++)
	{
		ufbx_node* node = scene->nodes.data[nodeIndex];
		if (!node->mesh)
			continue;

		ufbx_mesh* mesh = node->mesh;
		ufbx_matrix toWorld = node->geometry_to_world;
		ufbx_matrix normalToWorld = ufbx_matrix_for_normals(&toWorld);
		std::vector<uint32_t> triangleIndices(mesh->max_face_triangles * 3);

		for (size_t faceIndex = 0; faceIndex < mesh->faces.count; faceIndex++)
		{
			uint32_t triangleCount = ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), mesh, mesh->faces.data[faceIndex]);
			for (uint32_t corner = 0; corner < triangleCount * 3; corner++)
			{
				uint32_t index = triangleIndices[corner];
				Vertex vertex = {};

				ufbx_vec3 position = ufbx_transform_position(&toWorld, ufbx_get_vertex_vec3(&mesh->vertex_position, index));
				vertex.position = { position.x, position.y, position.z };
				vertex.color = { 1.f, 1.f, 1.f };

				if (mesh->vertex_normal.exists)
				{
					ufbx_vec3 normal = ufbx_transform_direction(&normalToWorld, ufbx_get_vertex_vec3(&mesh->vertex_normal, index));
					glm::vec3 n = { normal.x, normal.y, normal.z };
					vertex.normal = glm::length(n) > 0.f ? glm::normalize(n) : glm::vec3(0.f, 1.f, 0.f);
				}
				else
					vertex.normal = { 0.f, 1.f, 0.f };

				if (mesh->vertex_uv.exists)
				{
					ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
					vertex.texCoord = { uv.x, 1.f - uv.y };
				}

				AddVertex(vertex, uniqueVertices);
			}
		}
	}

	ufbx_free_scene(scene);
}

void Model::ApplyImportOptions(const ModelImportOptions& options)
{
	if (options.normalize)
	{
		glm::vec3 minBounds(FLT_MAX), maxBounds(-FLT_MAX);
		for (const auto& vertex : m_vertices)
		{
			minBounds = glm::min(minBounds, vertex.position);
			maxBounds = glm::max(maxBounds, vertex.position);
		}

		glm::vec3 center = (minBounds + maxBounds) * 0.5f;
		glm::vec3 size = maxBounds - minBounds;
		float scale = 2.f / std::max({ size.x, size.y, size.z });

		for (auto& vertex : m_vertices)
			vertex.position = (vertex.position - center) * scale;
		return;
	}

	glm::quat rotation = glm::quat(glm::radians(options.rotationDegrees));
	for (auto& vertex : m_vertices)
	{
		vertex.position = rotation * (vertex.position * options.scale) + options.offset;
		vertex.normal = rotation * vertex.normal;
	}
}

Model::Model(const std::string& modelName, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	: m_name(modelName), m_vertices(vertices), m_indices(indices)
{
	if (modelName.empty() || vertices.empty() || indices.empty())
		throw std::runtime_error("Model parameters not valid!");

	CalculateBounds();
}

Model::~Model()
{
}

void Model::CalculateBounds()
{
	m_boundsMin = glm::vec3(FLT_MAX);
	m_boundsMax = glm::vec3(-FLT_MAX);
	for (const auto& vertex : m_vertices)
	{
		m_boundsMin = glm::min(m_boundsMin, vertex.position);
		m_boundsMax = glm::max(m_boundsMax, vertex.position);
	}
}

const std::string& Model::GetName() const
{
	return m_name;
}

glm::vec3 Model::GetBoundsMin() const
{
	return m_boundsMin;
}

glm::vec3 Model::GetBoundsMax() const
{
	return m_boundsMax;
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