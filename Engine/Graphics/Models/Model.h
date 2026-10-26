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
	glm::vec3 normal;

	bool operator==(const Vertex& rhs) const {
		return position == rhs.position && color == rhs.color && texCoord == rhs.texCoord && normal == rhs.normal;
	}
};

namespace std
{
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1)) >> 1) ^
				(hash<glm::vec3>()(vertex.normal) << 1);
		}
	};
}

// How a model file is brought into the engine. Applied in order: scale, rotate, offset.
struct ModelImportOptions
{
	// Centre the model and fit it into a 2 unit cube, ignoring the other options.
	bool normalize = false;
	// Multiplier to metres, e.g. 0.001 for a model authored in millimetres.
	float scale = 1.f;
	glm::vec3 rotationDegrees = {};
	glm::vec3 offset = {};
};

class Model
{
	friend class ModelManager;
public:
	// Loads an .obj file, centred and scaled to fit a 2 unit cube.
	Model(const std::string& modelName, const std::string& modelPath);
	// Loads an .obj or .fbx file with explicit import options. FBX files are converted to
	// Y-up, right-handed, metres before the options are applied.
	Model(const std::string& modelName, const std::string& modelPath, const ModelImportOptions& options);
	// Wraps geometry built in code (see MeshBuilder), used as-is.
	Model(const std::string& modelName, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	~Model();
	const std::vector<Vertex>& GetVertices() const;
	const std::vector<uint32_t>& GetIndices() const;
	const uint32_t GetIndex() const;
	void SetIndex(const uint32_t& index);
	const std::string& GetName() const;

	// Local-space bounding box, used for picking.
	glm::vec3 GetBoundsMin() const;
	glm::vec3 GetBoundsMax() const;
private:
	void LoadObj();
	void LoadFbx();
	void AddVertex(const Vertex& vertex, std::unordered_map<Vertex, uint32_t>& uniqueVertices);
	void ApplyImportOptions(const ModelImportOptions& options);
	void CalculateBounds();

	std::string				m_name = "";
	uint32_t				m_index = 0;

	std::string				m_path = "";

	std::vector<Vertex>		m_vertices = {};
	std::vector<uint32_t>	m_indices = {};

	glm::vec3				m_boundsMin = {};
	glm::vec3				m_boundsMax = {};
};

typedef std::shared_ptr<Model> pModel;