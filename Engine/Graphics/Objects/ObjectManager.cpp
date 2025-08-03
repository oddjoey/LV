#include "ObjectManager.h"

std::shared_ptr<ObjectManager> ObjectManager::s_instance = nullptr;

std::shared_ptr<ObjectManager> ObjectManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<ObjectManager>(new ObjectManager());

	return s_instance;
}

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
}
