#pragma once

#include <unordered_map>
#include <memory>

#include "Object.h"

class ObjectManager
{
public:
	static std::shared_ptr<ObjectManager> Get();

	ObjectManager();
	~ObjectManager();
private:
	static std::shared_ptr<ObjectManager> s_instance;

	std::unordered_map<uint8_t, std::vector<Object>> m_modelInstances;
};