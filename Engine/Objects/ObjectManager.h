#pragma once

#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>

#include "Object.h"
#include "../Utility/Utility.h"

class Model;
struct MeshBvh;

struct RaycastHit
{
	pObject object = nullptr;
	float distance = 0.f;
	glm::vec3 point = {};
};

class ObjectManager
{
	CLASS_SINGLETON_HEADER(ObjectManager)
public:
	ObjectManager();
	~ObjectManager();
	pObject CreateObject(const Object& objInfo);
	// Closest visible, pickable object the ray hits. direction must be normalized. Exact tests the
	// model's triangles (objects thinner than a few centimetres still count anywhere in their box, so
	// they stay easy to aim at); otherwise only the model's bounding box is tested.
	std::optional<RaycastHit> Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance,
		const std::function<bool(const pObject&)>& filter = nullptr, bool exact = true) const;
	void AddObject(const pObject& obj = nullptr);
	void RemoveObject(const pObject& obj);
	void ChangeObjectModel(const pObject& object, const uint32_t newModelIndex);
	// By reference: the renderer walks it every frame.
	const std::unordered_map<uint32_t, std::vector<pObject>>& GetObjectsByModel() const;
	std::vector<pObject> GetObjects() const;
private:
	std::unordered_map<uint32_t, std::vector<pObject>> m_objectsByModel = {};
	std::vector<pObject> m_objects = {};
	// Each model's triangles sorted into a bounding volume tree the first time a ray tests them.
	mutable std::unordered_map<const Model*, std::shared_ptr<const MeshBvh>> m_meshBvhs = {};
};