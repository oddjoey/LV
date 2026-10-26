#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Model.h"

// Builds models out of primitives in code. Every primitive is placed through the current
// transform and coloured with the current colour, so composite shapes (a workbench, a
// crankshaft) are a series of SetTransform/Add calls. UVs are planar in world units scaled
// by the UV scale, so tiling textures stay the same size across differently sized parts.
class MeshBuilder
{
public:
	MeshBuilder& SetTransform(const glm::mat4& transform);
	MeshBuilder& ResetTransform();
	MeshBuilder& SetColor(const glm::vec3& color);
	// Texture repeats per unit of length.
	MeshBuilder& SetUVScale(const float& uvScale);

	MeshBuilder& AddBox(const glm::vec3& center, const glm::vec3& size);
	// Solid cylinder along local Y, centred on center.
	MeshBuilder& AddCylinder(const glm::vec3& center, const float& radius, const float& height, const int& segments = 24);
	// Hollow cylinder along local Y. A partial arc (radians) gives shapes like bearing shells.
	MeshBuilder& AddTube(const glm::vec3& center, const float& outerRadius, const float& innerRadius, const float& height,
		const int& segments = 24, const float& arcStart = 0.f, const float& arcEnd = glm::two_pi<float>());
	// Upward-facing rectangle with a round hole in the middle, e.g. a block deck around one bore.
	MeshBuilder& AddPlateWithHole(const glm::vec3& center, const glm::vec2& size, const float& holeRadius, const int& segments = 24);
	// A round wire wound into a coil along local Y, starting at base, e.g. a valve spring.
	MeshBuilder& AddHelix(const glm::vec3& base, const float& coilRadius, const float& wireRadius, const float& height, const float& turns,
		const int& segmentsPerTurn = 16, const int& wireSegments = 6);
	// UV sphere. inward flips it to be seen from inside, e.g. a sky dome around the camera.
	MeshBuilder& AddSphere(const glm::vec3& center, const float& radius, const int& rings = 8, const int& segments = 12, const bool& inward = false);
	// Cone along local Y with its base centred on base, e.g. a conifer or a traffic cone.
	MeshBuilder& AddCone(const glm::vec3& base, const float& radius, const float& height, const int& segments = 12);
	// One flat triangle, facing the side from which a, b, c run counter-clockwise. UVs are planar.
	MeshBuilder& AddFlatTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

	const std::vector<Vertex>& GetVertices() const;
	const std::vector<uint32_t>& GetIndices() const;
	Model Build(const std::string& name) const;
private:
	void AddTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
		const glm::vec2& uvA, const glm::vec2& uvB, const glm::vec2& uvC, const glm::vec3& normal);
	void AddQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normal, const glm::vec2& size);

	std::vector<Vertex>		m_vertices = {};
	std::vector<uint32_t>	m_indices = {};
	glm::mat4				m_transform = glm::mat4(1.f);
	glm::vec3				m_color = { 1.f, 1.f, 1.f };
	float					m_uvScale = 1.f;
};
