#pragma once

#include <unordered_map>
#include <memory>

#include "Object.h"
#include "../Utility/Utility.h"

class ObjectManager
{
	CLASS_SINGLETON_HEADER(ObjectManager)
public:
	ObjectManager();
	~ObjectManager();
	pObject CreateObject(const Object& objInfo);
	void AddObject(const pObject& obj = nullptr);
	void RemoveObject(const pObject& obj);
	void ChangeObjectModel(const pObject& object, const uint32_t newModelIndex);
	std::unordered_map<uint32_t, std::vector<pObject>> GetObjectsByModel() const;
	std::vector<pObject> GetObjects() const;
private:
	std::unordered_map<uint32_t, std::vector<pObject>> m_objectsByModel = {};
	std::vector<pObject> m_objects = {};
};