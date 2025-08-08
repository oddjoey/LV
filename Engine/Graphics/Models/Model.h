#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;
};

class Model
{
	friend class ModelManager;
public:
	Model(const std::string& modelName, const std::string& modelPath);
	~Model();
	const std::vector<Vertex>& GetVertices() const;
	const std::vector<uint32_t>& GetIndices() const;
	const uint8_t GetIndex() const;
	void SetIndex(const uint8_t& index);
private:
	std::string				m_name = "";
	uint8_t					m_index = 0;

	std::string				m_path = "";

	std::vector<Vertex>		m_vertices = {};
	std::vector<uint32_t>	m_indices = {};
};