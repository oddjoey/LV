#include "ObjectManager.h"

#include "../Graphics/Models/ModelManager.h"

CLASS_SINGLETON_SOURCE(ObjectManager)

namespace
{
	// Slab test; returns the entry distance along the ray, or nothing on a miss.
	std::optional<float> IntersectBox(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& boxMin, const glm::vec3& boxMax)
	{
		float tMin = 0.f;
		float tMax = FLT_MAX;
		for (int axis = 0; axis < 3; axis++)
		{
			if (std::abs(direction[axis]) < 1e-8f)
			{
				if (origin[axis] < boxMin[axis] || origin[axis] > boxMax[axis])
					return std::nullopt;
				continue;
			}

			float inverse = 1.f / direction[axis];
			float t0 = (boxMin[axis] - origin[axis]) * inverse;
			float t1 = (boxMax[axis] - origin[axis]) * inverse;
			if (t0 > t1)
				std::swap(t0, t1);
			// Parenthesized to dodge the windows.h min/max macros.
			tMin = (std::max)(tMin, t0);
			tMax = (std::min)(tMax, t1);
			if (tMin > tMax)
				return std::nullopt;
		}
		return tMin;
	}

	// Ray against a box given the ray's reciprocal direction: the entry distance, if it's crossed
	// before limit.
	bool CrossesBox(const glm::vec3& origin, const glm::vec3& inverseDirection, const glm::vec3& boxMin, const glm::vec3& boxMax, float limit, float& entry)
	{
		float tMin = 0.f, tMax = limit;
		for (int axis = 0; axis < 3; axis++)
		{
			float t0 = (boxMin[axis] - origin[axis]) * inverseDirection[axis];
			float t1 = (boxMax[axis] - origin[axis]) * inverseDirection[axis];
			if (t0 > t1)
				std::swap(t0, t1);
			tMin = t0 > tMin ? t0 : tMin;
			tMax = t1 < tMax ? t1 : tMax;
			if (tMin > tMax)
				return false;
		}
		entry = tMin;
		return true;
	}

	// The ray's distance to a triangle (either side), Moller-Trumbore, if it crosses it.
	bool CrossesTriangle(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
	{
		glm::vec3 edge1 = b - a;
		glm::vec3 edge2 = c - a;
		glm::vec3 p = glm::cross(direction, edge2);
		float determinant = glm::dot(edge1, p);
		if (std::abs(determinant) < 1e-12f)
			return false;
		float inverse = 1.f / determinant;
		glm::vec3 toOrigin = origin - a;
		float u = glm::dot(toOrigin, p) * inverse;
		if (u < 0.f || u > 1.f)
			return false;
		glm::vec3 q = glm::cross(toOrigin, edge1);
		float v = glm::dot(direction, q) * inverse;
		if (v < 0.f || u + v > 1.f)
			return false;
		t = glm::dot(edge2, q) * inverse;
		return t >= 0.f;
	}
}

// A model's triangles in a bounding volume tree, so a ray only tests the few near its path instead
// of all of them (an engine block has thousands, and the spanner casts rays at every bolt in view).
struct MeshBvh
{
	struct Node
	{
		glm::vec3 min, max;
		uint32_t first = 0;		// leaves: the first triangle; inner nodes: the second child (the first follows)
		uint32_t count = 0;		// triangles in a leaf, 0 for an inner node
	};
	std::vector<Node> nodes;
	std::vector<glm::vec3> corners;	// three per triangle, in the leaves' order

	explicit MeshBvh(const Model& model)
	{
		const auto& vertices = model.GetVertices();
		const auto& indices = model.GetIndices();
		size_t triangleCount = indices.size() / 3;
		std::vector<uint32_t> order(triangleCount);
		std::vector<glm::vec3> centres(triangleCount);
		for (uint32_t i = 0; i < triangleCount; i++)
		{
			order[i] = i;
			centres[i] = (vertices[indices[i * 3]].position + vertices[indices[i * 3 + 1]].position + vertices[indices[i * 3 + 2]].position) / 3.f;
		}
		if (triangleCount)
			Build(vertices, indices, centres, order, 0, static_cast<uint32_t>(triangleCount));
		corners.reserve(triangleCount * 3);
		for (uint32_t triangle : order)
			for (int corner = 0; corner < 3; corner++)
				corners.push_back(vertices[indices[triangle * 3 + corner]].position);
	}

	// Splits [begin, end) at the middle of its longest side until a handful are left.
	void Build(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<glm::vec3>& centres,
		std::vector<uint32_t>& order, uint32_t begin, uint32_t end)
	{
		constexpr uint32_t LEAF_SIZE = 6;
		uint32_t index = static_cast<uint32_t>(nodes.size());
		nodes.push_back({ glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX) });
		glm::vec3 boxMin(FLT_MAX), boxMax(-FLT_MAX), centreMin(FLT_MAX), centreMax(-FLT_MAX);
		for (uint32_t i = begin; i < end; i++)
		{
			for (int corner = 0; corner < 3; corner++)
			{
				const glm::vec3& position = vertices[indices[order[i] * 3 + corner]].position;
				boxMin = glm::min(boxMin, position);
				boxMax = glm::max(boxMax, position);
			}
			centreMin = glm::min(centreMin, centres[order[i]]);
			centreMax = glm::max(centreMax, centres[order[i]]);
		}
		nodes[index].min = boxMin;
		nodes[index].max = boxMax;

		if (end - begin <= LEAF_SIZE)
		{
			nodes[index].first = begin;
			nodes[index].count = end - begin;
			return;
		}
		glm::vec3 extent = centreMax - centreMin;
		int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
		uint32_t middle = begin + (end - begin) / 2;
		std::nth_element(order.begin() + begin, order.begin() + middle, order.begin() + end,
			[&](uint32_t a, uint32_t b) { return centres[a][axis] < centres[b][axis]; });
		Build(vertices, indices, centres, order, begin, middle);
		nodes[index].first = static_cast<uint32_t>(nodes.size());
		Build(vertices, indices, centres, order, middle, end);
	}

	// Closest triangle the ray crosses before limit. The direction needn't be normalized: the
	// distance comes back in its units.
	std::optional<float> Intersect(const glm::vec3& origin, const glm::vec3& direction, float limit) const
	{
		if (nodes.empty())
			return std::nullopt;
		glm::vec3 inverseDirection;
		for (int axis = 0; axis < 3; axis++)
			inverseDirection[axis] = std::abs(direction[axis]) > 1e-12f ? 1.f / direction[axis] : (direction[axis] < 0.f ? -1e30f : 1e30f);

		std::optional<float> closest;
		uint32_t stack[64];
		int depth = 0;
		stack[depth++] = 0;
		while (depth)
		{
			const Node& node = nodes[stack[--depth]];
			float entry;
			if (!CrossesBox(origin, inverseDirection, node.min, node.max, closest ? *closest : limit, entry))
				continue;
			if (node.count)
			{
				for (uint32_t i = node.first; i < node.first + node.count; i++)
				{
					float t;
					if (CrossesTriangle(origin, direction, corners[i * 3], corners[i * 3 + 1], corners[i * 3 + 2], t) && t <= limit && (!closest || t < *closest))
						closest = t;
				}
			}
			else if (depth + 2 <= 64)
			{
				uint32_t first = static_cast<uint32_t>(&node - nodes.data()) + 1;
				stack[depth++] = node.first;
				stack[depth++] = first;
			}
		}
		return closest;
	}
};

std::optional<RaycastHit> ObjectManager::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
	const std::function<bool(const pObject&)>& filter, bool exact) const
{
	// Thinner than this (in metres, the box's middle side) and anywhere in the box counts as a hit
	// even when the ray is testing triangles: a bolt or a pushrod is hard enough to aim at as it is.
	constexpr float SMALL_OBJECT = 0.06f;

	struct Candidate
	{
		const pObject* object;
		const Model* model;
		float boxDistance;
		glm::vec3 localOrigin;
		glm::vec3 localDirection;
	};
	std::vector<Candidate> candidates;

	auto modelManager = ModelManager::Get();
	std::optional<RaycastHit> closest;

	for (const auto& object : m_objects)
	{
		if (!object->IsVisible() || !object->IsPickable() || !object->GetModelIndex())
			continue;
		const Model* model = modelManager->PeekModel(object->GetModelIndex());
		if (!model)
			continue;

		// Cheap reject first: a sphere round the box, in world space. Most objects are nowhere near
		// the ray, and this spares them the filter and the matrix inverse below.
		glm::vec3 scale = object->GetScale();
		float scaleMax = (std::max)({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
		glm::vec3 localCenter = (model->GetBoundsMin() + model->GetBoundsMax()) * 0.5f;
		float radius = glm::length(model->GetBoundsMax() - model->GetBoundsMin()) * 0.5f * scaleMax;
		glm::vec3 toCenter = object->GetPosition() + object->GetRotation() * (localCenter * scale) - origin;
		float along = glm::dot(toCenter, direction);
		if (along < -radius || along > maxDistance + radius || glm::dot(toCenter, toCenter) - along * along > radius * radius)
			continue;

		if (filter && !filter(object))
			continue;

		// Test in the object's own space so rotated objects get a tight box. The ray parameter is
		// unchanged by the transform, so the hit distance is still in world units.
		// The model matrix is translate * rotate * scale, so undoing it is cheaper than a general inverse.
		if (scale.x == 0.f || scale.y == 0.f || scale.z == 0.f)
			continue;
		glm::quat unrotate = glm::conjugate(glm::normalize(object->GetRotation()));
		glm::vec3 localOrigin = (unrotate * (origin - object->GetPosition())) / scale;
		glm::vec3 localDirection = (unrotate * direction) / scale;

		auto distance = IntersectBox(localOrigin, localDirection, model->GetBoundsMin(), model->GetBoundsMax());
		if (!distance || *distance > maxDistance)
			continue;

		if (exact)
			candidates.push_back({ &object, model, *distance, localOrigin, localDirection });
		else if (!closest || *distance < closest->distance)
			closest = RaycastHit{ object, *distance, origin + direction * *distance };
	}

	if (!exact)
		return closest;

	// Nearest box first, and stop once a box starts beyond the nearest hit: a big part behind a small
	// one has a box reaching out round it, and only the triangles say which one is really in front.
	std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.boxDistance < b.boxDistance; });
	for (const auto& candidate : candidates)
	{
		if (closest && candidate.boxDistance >= closest->distance)
			break;

		// Thin is what makes it hard to hit: a pushrod is long, but only its box is wide enough to aim at.
		glm::vec3 size = glm::abs((*candidate.object)->GetScale() * (candidate.model->GetBoundsMax() - candidate.model->GetBoundsMin()));
		float sides[3] = { size.x, size.y, size.z };
		std::sort(std::begin(sides), std::end(sides));
		std::optional<float> distance;
		if (sides[1] < SMALL_OBJECT)
			distance = candidate.boxDistance;
		else
		{
			// Rebuilt if a different model has since been loaded at the same address.
			auto& bvh = m_meshBvhs[candidate.model];
			if (!bvh || bvh->corners.size() != candidate.model->GetIndices().size() / 3 * 3)
				bvh = std::make_shared<const MeshBvh>(*candidate.model);
			distance = bvh->Intersect(candidate.localOrigin, candidate.localDirection, closest ? closest->distance : maxDistance);
		}
		if (distance && (!closest || *distance < closest->distance))
			closest = RaycastHit{ *candidate.object, *distance, origin + direction * *distance };
	}

	return closest;
}

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
}


pObject ObjectManager::CreateObject(const Object& objInfo)
{
	auto object = std::make_shared<Object>(objInfo);
	AddObject(object);
	return object;
}

void ObjectManager::AddObject(const pObject& obj)
{
	m_objectsByModel[obj->m_modelIndex].push_back(obj);
	m_objects.push_back(obj);
	obj->m_index = static_cast<uint32_t>(m_objects.size());
}

void ObjectManager::RemoveObject(const pObject& obj)
{
	auto& objectList = m_objectsByModel[obj->GetModelIndex()];
	objectList.erase(std::remove(objectList.begin(), objectList.end(), obj), objectList.end());

	m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), obj), m_objects.end());

	for (uint32_t i = 0; i < m_objects.size(); i++)
		m_objects[i]->m_index = i + 1;
}

void ObjectManager::ChangeObjectModel(const pObject& object, const uint32_t newModelIndex)
{
	auto oldModelIndex = object->GetModelIndex();
	if (oldModelIndex == newModelIndex)
		return;

	auto& modelBucket = m_objectsByModel[oldModelIndex];
	modelBucket.erase(std::remove(modelBucket.begin(), modelBucket.end(), object), modelBucket.end());

	object->SetModel(newModelIndex);
	m_objectsByModel[newModelIndex].push_back(object);
}

const std::unordered_map<uint32_t, std::vector<pObject>>& ObjectManager::GetObjectsByModel() const
{
	return m_objectsByModel;
}

std::vector<pObject> ObjectManager::GetObjects() const
{
	return m_objects;
}
