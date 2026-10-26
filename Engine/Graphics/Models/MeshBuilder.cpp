#include "MeshBuilder.h"

#include <algorithm>
#include <cmath>

MeshBuilder& MeshBuilder::SetTransform(const glm::mat4& transform)
{
	m_transform = transform;
	return *this;
}

MeshBuilder& MeshBuilder::ResetTransform()
{
	m_transform = glm::mat4(1.f);
	return *this;
}

MeshBuilder& MeshBuilder::SetColor(const glm::vec3& color)
{
	m_color = color;
	return *this;
}

MeshBuilder& MeshBuilder::SetUVScale(const float& uvScale)
{
	m_uvScale = uvScale;
	return *this;
}

void MeshBuilder::AddTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
	const glm::vec2& uvA, const glm::vec2& uvB, const glm::vec2& uvC, const glm::vec3& normal)
{
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m_transform)));
	glm::vec3 worldNormal = glm::normalize(normalMatrix * normal);

	glm::vec3 positions[3] = {
		glm::vec3(m_transform * glm::vec4(a, 1.f)),
		glm::vec3(m_transform * glm::vec4(b, 1.f)),
		glm::vec3(m_transform * glm::vec4(c, 1.f))
	};
	glm::vec2 uvs[3] = { uvA, uvB, uvC };

	// Front faces are counter-clockwise when seen from the side the normal points to; fix the
	// order here rather than relying on every caller to get it right.
	if (glm::dot(glm::cross(positions[1] - positions[0], positions[2] - positions[0]), worldNormal) < 0.f)
	{
		std::swap(positions[1], positions[2]);
		std::swap(uvs[1], uvs[2]);
	}

	auto base = static_cast<uint32_t>(m_vertices.size());
	for (int i = 0; i < 3; i++)
	{
		m_vertices.push_back({ positions[i], m_color, uvs[i], worldNormal });
		m_indices.push_back(base + i);
	}
}

void MeshBuilder::AddQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normal, const glm::vec2& size)
{
	glm::vec2 uvSize = size * m_uvScale;
	AddTriangle(a, b, c, { 0.f, 0.f }, { uvSize.x, 0.f }, { uvSize.x, uvSize.y }, normal);
	AddTriangle(a, c, d, { 0.f, 0.f }, { uvSize.x, uvSize.y }, { 0.f, uvSize.y }, normal);
}

MeshBuilder& MeshBuilder::AddBox(const glm::vec3& center, const glm::vec3& size)
{
	glm::vec3 h = size * 0.5f;
	glm::vec3 p[8] = {
		center + glm::vec3(-h.x, -h.y, -h.z), center + glm::vec3(h.x, -h.y, -h.z),
		center + glm::vec3(h.x, h.y, -h.z), center + glm::vec3(-h.x, h.y, -h.z),
		center + glm::vec3(-h.x, -h.y, h.z), center + glm::vec3(h.x, -h.y, h.z),
		center + glm::vec3(h.x, h.y, h.z), center + glm::vec3(-h.x, h.y, h.z)
	};

	AddQuad(p[4], p[5], p[6], p[7], { 0.f, 0.f, 1.f }, { size.x, size.y });	// front
	AddQuad(p[1], p[0], p[3], p[2], { 0.f, 0.f, -1.f }, { size.x, size.y });	// back
	AddQuad(p[5], p[1], p[2], p[6], { 1.f, 0.f, 0.f }, { size.z, size.y });	// right
	AddQuad(p[0], p[4], p[7], p[3], { -1.f, 0.f, 0.f }, { size.z, size.y });	// left
	AddQuad(p[7], p[6], p[2], p[3], { 0.f, 1.f, 0.f }, { size.x, size.z });	// top
	AddQuad(p[0], p[1], p[5], p[4], { 0.f, -1.f, 0.f }, { size.x, size.z });	// bottom
	return *this;
}

MeshBuilder& MeshBuilder::AddCylinder(const glm::vec3& center, const float& radius, const float& height, const int& segments)
{
	float halfHeight = height * 0.5f;
	glm::vec3 top = center + glm::vec3(0.f, halfHeight, 0.f);
	glm::vec3 bottom = center - glm::vec3(0.f, halfHeight, 0.f);
	float circumference = glm::two_pi<float>() * radius;

	for (int i = 0; i < segments; i++)
	{
		float a0 = glm::two_pi<float>() * i / segments;
		float a1 = glm::two_pi<float>() * (i + 1) / segments;
		glm::vec3 d0 = { std::cos(a0), 0.f, std::sin(a0) };
		glm::vec3 d1 = { std::cos(a1), 0.f, std::sin(a1) };
		float u0 = circumference * i / segments * m_uvScale;
		float u1 = circumference * (i + 1) / segments * m_uvScale;
		float v = height * m_uvScale;

		// Side, with smooth normals per vertex pair.
		glm::vec3 sideNormal = glm::normalize(d0 + d1);
		AddTriangle(bottom + d0 * radius, bottom + d1 * radius, top + d1 * radius, { u0, 0.f }, { u1, 0.f }, { u1, v }, sideNormal);
		AddTriangle(bottom + d0 * radius, top + d1 * radius, top + d0 * radius, { u0, 0.f }, { u1, v }, { u0, v }, sideNormal);

		glm::vec2 c = { 0.5f, 0.5f };
		glm::vec2 uv0 = c + glm::vec2(d0.x, d0.z) * radius * m_uvScale;
		glm::vec2 uv1 = c + glm::vec2(d1.x, d1.z) * radius * m_uvScale;
		AddTriangle(top, top + d0 * radius, top + d1 * radius, c, uv0, uv1, { 0.f, 1.f, 0.f });
		AddTriangle(bottom, bottom + d1 * radius, bottom + d0 * radius, c, uv1, uv0, { 0.f, -1.f, 0.f });
	}
	return *this;
}

MeshBuilder& MeshBuilder::AddTube(const glm::vec3& center, const float& outerRadius, const float& innerRadius, const float& height,
	const int& segments, const float& arcStart, const float& arcEnd)
{
	float halfHeight = height * 0.5f;
	glm::vec3 up = { 0.f, halfHeight, 0.f };
	float arc = arcEnd - arcStart;
	float v = height * m_uvScale;

	auto direction = [&](int i) {
		float angle = arcStart + arc * i / segments;
		return glm::vec3(std::cos(angle), 0.f, std::sin(angle));
	};

	for (int i = 0; i < segments; i++)
	{
		glm::vec3 d0 = direction(i), d1 = direction(i + 1);
		glm::vec3 sideNormal = glm::normalize(d0 + d1);
		float u0 = arc * outerRadius * i / segments * m_uvScale;
		float u1 = arc * outerRadius * (i + 1) / segments * m_uvScale;

		glm::vec3 outer0 = center + d0 * outerRadius, outer1 = center + d1 * outerRadius;
		glm::vec3 inner0 = center + d0 * innerRadius, inner1 = center + d1 * innerRadius;

		// Outer wall faces out, inner wall faces in.
		AddTriangle(outer0 - up, outer1 - up, outer1 + up, { u0, 0.f }, { u1, 0.f }, { u1, v }, sideNormal);
		AddTriangle(outer0 - up, outer1 + up, outer0 + up, { u0, 0.f }, { u1, v }, { u0, v }, sideNormal);
		AddTriangle(inner0 - up, inner1 + up, inner1 - up, { u0, 0.f }, { u1, v }, { u1, 0.f }, -sideNormal);
		AddTriangle(inner0 - up, inner0 + up, inner1 + up, { u0, 0.f }, { u0, v }, { u1, v }, -sideNormal);

		// Annular caps.
		float ringWidth = (outerRadius - innerRadius) * m_uvScale;
		AddTriangle(inner0 + up, outer0 + up, outer1 + up, { u0, 0.f }, { u0, ringWidth }, { u1, ringWidth }, { 0.f, 1.f, 0.f });
		AddTriangle(inner0 + up, outer1 + up, inner1 + up, { u0, 0.f }, { u1, ringWidth }, { u1, 0.f }, { 0.f, 1.f, 0.f });
		AddTriangle(inner0 - up, outer1 - up, outer0 - up, { u0, 0.f }, { u1, ringWidth }, { u0, ringWidth }, { 0.f, -1.f, 0.f });
		AddTriangle(inner0 - up, inner1 - up, outer1 - up, { u0, 0.f }, { u1, 0.f }, { u1, ringWidth }, { 0.f, -1.f, 0.f });
	}

	// Close the ends of a partial arc.
	if (arc < glm::two_pi<float>() - 1e-4f)
	{
		glm::vec2 endSize = { outerRadius - innerRadius, height };
		glm::vec3 startDirection = direction(0), endDirection = direction(segments);
		glm::vec3 startNormal = glm::normalize(glm::cross(glm::vec3(0.f, 1.f, 0.f), startDirection));
		AddQuad(center + startDirection * innerRadius - up, center + startDirection * outerRadius - up,
			center + startDirection * outerRadius + up, center + startDirection * innerRadius + up, startNormal, endSize);
		AddQuad(center + endDirection * innerRadius - up, center + endDirection * outerRadius - up,
			center + endDirection * outerRadius + up, center + endDirection * innerRadius + up, -glm::normalize(glm::cross(glm::vec3(0.f, 1.f, 0.f), endDirection)), endSize);
	}
	return *this;
}

MeshBuilder& MeshBuilder::AddPlateWithHole(const glm::vec3& center, const glm::vec2& size, const float& holeRadius, const int& segments)
{
	glm::vec2 half = size * 0.5f;
	glm::vec3 normal = { 0.f, 1.f, 0.f };

	// Pair each point on the hole with the point on the rectangle edge at the same angle.
	auto edgePoint = [&](float angle) {
		glm::vec2 d = { std::cos(angle), std::sin(angle) };
		float scale = std::min(half.x / std::max(std::abs(d.x), 1e-6f), half.y / std::max(std::abs(d.y), 1e-6f));
		return d * scale;
	};
	auto toWorld = [&](const glm::vec2& p) { return center + glm::vec3(p.x, 0.f, p.y); };
	auto toUV = [&](const glm::vec2& p) { return (p + half) * m_uvScale; };

	// Corners must be hit exactly or the plate gets notches, so include their angles.
	std::vector<float> angles;
	for (int i = 0; i <= segments; i++)
		angles.push_back(glm::two_pi<float>() * i / segments);
	for (float cornerAngle : { std::atan2(half.y, half.x), std::atan2(half.y, -half.x), std::atan2(-half.y, -half.x), std::atan2(-half.y, half.x) })
		angles.push_back(cornerAngle < 0.f ? cornerAngle + glm::two_pi<float>() : cornerAngle);
	std::sort(angles.begin(), angles.end());

	for (size_t i = 0; i + 1 < angles.size(); i++)
	{
		glm::vec2 hole0 = glm::vec2(std::cos(angles[i]), std::sin(angles[i])) * holeRadius;
		glm::vec2 hole1 = glm::vec2(std::cos(angles[i + 1]), std::sin(angles[i + 1])) * holeRadius;
		glm::vec2 edge0 = edgePoint(angles[i]);
		glm::vec2 edge1 = edgePoint(angles[i + 1]);

		AddTriangle(toWorld(hole0), toWorld(edge0), toWorld(edge1), toUV(hole0), toUV(edge0), toUV(edge1), normal);
		AddTriangle(toWorld(hole0), toWorld(edge1), toWorld(hole1), toUV(hole0), toUV(edge1), toUV(hole1), normal);
	}
	return *this;
}

MeshBuilder& MeshBuilder::AddHelix(const glm::vec3& base, const float& coilRadius, const float& wireRadius, const float& height, const float& turns,
	const int& segmentsPerTurn, const int& wireSegments)
{
	int pathSegments = (std::max)(1, static_cast<int>(turns * segmentsPerTurn));
	float angularSpeed = turns * glm::two_pi<float>();

	// Sweep a circle of wire along the coil, using the coil's radial direction as the frame normal.
	auto ring = [&](int step, std::vector<glm::vec3>& points, std::vector<glm::vec3>& normals)
	{
		float t = static_cast<float>(step) / pathSegments;
		float angle = t * angularSpeed;
		glm::vec3 radial = { std::cos(angle), 0.f, std::sin(angle) };
		glm::vec3 center = base + radial * coilRadius + glm::vec3(0.f, t * height, 0.f);
		glm::vec3 tangent = glm::normalize(glm::vec3(-std::sin(angle) * coilRadius * angularSpeed, height, std::cos(angle) * coilRadius * angularSpeed));
		glm::vec3 binormal = glm::normalize(glm::cross(tangent, radial));

		points.clear();
		normals.clear();
		for (int i = 0; i <= wireSegments; i++)
		{
			float phi = glm::two_pi<float>() * i / wireSegments;
			glm::vec3 direction = radial * std::cos(phi) + binormal * std::sin(phi);
			points.push_back(center + direction * wireRadius);
			normals.push_back(direction);
		}
	};

	std::vector<glm::vec3> previousPoints, previousNormals, points, normals;
	ring(0, previousPoints, previousNormals);
	for (int step = 1; step <= pathSegments; step++)
	{
		ring(step, points, normals);
		for (int i = 0; i < wireSegments; i++)
		{
			glm::vec3 normal = glm::normalize(previousNormals[i] + previousNormals[i + 1] + normals[i] + normals[i + 1]);
			AddTriangle(previousPoints[i], previousPoints[i + 1], points[i + 1], { 0.f, 0.f }, { 1.f, 0.f }, { 1.f, 1.f }, normal);
			AddTriangle(previousPoints[i], points[i + 1], points[i], { 0.f, 0.f }, { 1.f, 1.f }, { 0.f, 1.f }, normal);
		}
		std::swap(previousPoints, points);
		std::swap(previousNormals, normals);
	}
	return *this;
}

const std::vector<Vertex>& MeshBuilder::GetVertices() const
{
	return m_vertices;
}

const std::vector<uint32_t>& MeshBuilder::GetIndices() const
{
	return m_indices;
}

Model MeshBuilder::Build(const std::string& name) const
{
	return Model(name, m_vertices, m_indices);
}

MeshBuilder& MeshBuilder::AddSphere(const glm::vec3& center, const float& radius, const int& rings, const int& segments, const bool& inward)
{
	auto point = [&](int ring, int segment) {
		float polar = glm::pi<float>() * ring / rings;
		float azimuth = glm::two_pi<float>() * segment / segments;
		return glm::vec3(std::sin(polar) * std::cos(azimuth), std::cos(polar), std::sin(polar) * std::sin(azimuth));
	};
	float flip = inward ? -1.f : 1.f;
	float circumference = glm::two_pi<float>() * radius * m_uvScale;
	float meridian = glm::pi<float>() * radius * m_uvScale;

	for (int ring = 0; ring < rings; ring++)
		for (int segment = 0; segment < segments; segment++)
		{
			glm::vec3 a = point(ring, segment), b = point(ring, segment + 1), c = point(ring + 1, segment + 1), d = point(ring + 1, segment);
			glm::vec2 uvA = { circumference * segment / segments, meridian * ring / rings };
			glm::vec2 uvC = { circumference * (segment + 1) / segments, meridian * (ring + 1) / rings };
			glm::vec3 normal = glm::normalize(a + b + c + d) * flip;
			if (ring > 0)
				AddTriangle(center + a * radius, center + b * radius, center + c * radius, uvA, { uvC.x, uvA.y }, uvC, normal);
			if (ring < rings - 1)
				AddTriangle(center + a * radius, center + c * radius, center + d * radius, uvA, uvC, { uvA.x, uvC.y }, normal);
		}
	return *this;
}

MeshBuilder& MeshBuilder::AddCone(const glm::vec3& base, const float& radius, const float& height, const int& segments)
{
	glm::vec3 tip = base + glm::vec3(0.f, height, 0.f);
	float slant = std::sqrt(radius * radius + height * height);
	for (int i = 0; i < segments; i++)
	{
		float a0 = glm::two_pi<float>() * i / segments;
		float a1 = glm::two_pi<float>() * (i + 1) / segments;
		glm::vec3 d0 = { std::cos(a0), 0.f, std::sin(a0) };
		glm::vec3 d1 = { std::cos(a1), 0.f, std::sin(a1) };
		glm::vec3 outward = glm::normalize(d0 + d1);
		glm::vec3 normal = glm::normalize(outward * height + glm::vec3(0.f, radius, 0.f));
		float u0 = glm::two_pi<float>() * radius * i / segments * m_uvScale;
		float u1 = glm::two_pi<float>() * radius * (i + 1) / segments * m_uvScale;
		AddTriangle(base + d0 * radius, base + d1 * radius, tip, { u0, 0.f }, { u1, 0.f }, { (u0 + u1) * 0.5f, slant * m_uvScale }, normal);
		AddTriangle(base, base + d1 * radius, base + d0 * radius, { 0.5f, 0.5f }, { 0.5f + d1.x * radius, 0.5f + d1.z * radius },
			{ 0.5f + d0.x * radius, 0.5f + d0.z * radius }, { 0.f, -1.f, 0.f });
	}
	return *this;
}

MeshBuilder& MeshBuilder::AddFlatTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
	glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
	glm::vec3 uAxis = glm::normalize(b - a);
	glm::vec3 vAxis = glm::cross(normal, uAxis);
	auto uv = [&](const glm::vec3& p) { return glm::vec2(glm::dot(p - a, uAxis), glm::dot(p - a, vAxis)) * m_uvScale; };
	AddTriangle(a, b, c, uv(a), uv(b), uv(c), normal);
	return *this;
}
