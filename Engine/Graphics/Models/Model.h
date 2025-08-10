#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;

	bool operator==(const Vertex& rhs) const {
		return position == rhs.position && color == rhs.color && texCoord == rhs.texCoord;
	}
};

namespace std 
{
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

class Model
{
	friend class ModelManager;
public:
	Model(const std::string& modelName, const std::string& modelPath);
	~Model();
	const std::vector<Vertex>& GetVertices() const;
	const std::vector<uint32_t>& GetIndices() const;
	const uint32_t GetIndex() const;
	void SetIndex(const uint32_t& index);
private:
	std::string				m_name = "";
	uint32_t				m_index = 0;

	std::string				m_path = "";

	std::vector<Vertex>		m_vertices = {};
	std::vector<uint32_t>	m_indices = {};
};

typedef std::shared_ptr<Model> pModel;