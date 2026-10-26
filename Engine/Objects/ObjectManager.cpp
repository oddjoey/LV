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
}

std::optional<RaycastHit> ObjectManager::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
	const std::function<bool(const pObject&)>& filter) const
{
	auto modelManager = ModelManager::Get();
	std::optional<RaycastHit> closest;

	for (const auto& object : m_objects)
	{
		if (!object->IsVisible() || !object->IsPickable() || !object->GetModelIndex())
			continue;
		if (filter && !filter(object))
			continue;

		auto model = modelManager->GetModel(object->GetModelIndex());
		if (!model)
			continue;

		// Test in the object's own space so rotated objects get a tight box. The ray parameter is
		// unchanged by the transform, so the hit distance is still in world units.
		glm::mat4 inverse = glm::inverse(object->GetModelMatrix());
		glm::vec3 localOrigin = glm::vec3(inverse * glm::vec4(origin, 1.f));
		glm::vec3 localDirection = glm::vec3(inverse * glm::vec4(direction, 0.f));

		auto distance = IntersectBox(localOrigin, localDirection, model->GetBoundsMin(), model->GetBoundsMax());
		if (!distance || *distance > maxDistance)
			continue;

		if (!closest || *distance < closest->distance)
			closest = RaycastHit{ object, *distance, origin + direction * *distance };
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

std::unordered_map<uint32_t, std::vector<pObject>> ObjectManager::GetObjectsByModel() const
{
	return m_objectsByModel;
}

std::vector<pObject> ObjectManager::GetObjects() const
{
	return m_objects;
}
