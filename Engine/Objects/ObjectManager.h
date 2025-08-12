#pragma once

#include <unordered_map>
#include <memory>

#include "Object.h"

class ObjectManager
{
public:
	static std::shared_ptr<ObjectManager> Get();
	pObject CreateObject(const Object& objInfo);
	void AddObject(const pObject& obj);
	void ChangeObjectModel(const pObject& object, const uint32_t newModelIndex);
	std::unordered_map<uint32_t, std::vector<pObject>> GetObjects() const;
	ObjectManager();
	~ObjectManager();
private:
	static std::shared_ptr<ObjectManager> s_instance;
	
	std::unordered_map<uint32_t, std::vector<pObject>> m_objects = {};
};