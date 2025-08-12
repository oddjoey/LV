#include "ObjectManager.h"

std::shared_ptr<ObjectManager> ObjectManager::s_instance = nullptr;

std::shared_ptr<ObjectManager> ObjectManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<ObjectManager>(new ObjectManager());

	return s_instance;
}

pObject ObjectManager::CreateObject(const Object& objInfo)
{
	auto object = std::make_shared<Object>(objInfo);
	AddObject(object);
	return object;
}

void ObjectManager::AddObject(const pObject& obj)
{
	m_objects[obj->m_modelIndex].push_back(obj);
	obj->m_index = static_cast<uint32_t>(m_objects.size());
}

void ObjectManager::ChangeObjectModel(const pObject& object, const uint32_t newModelIndex)
{
	auto oldModelIndex = object->GetModelIndex();
	if (oldModelIndex == newModelIndex)
		return;

	auto& modelBucket = m_objects[oldModelIndex];
	modelBucket.erase(std::remove(modelBucket.begin(), modelBucket.end(), object), modelBucket.end());

	object->SetModel(newModelIndex);
	m_objects[newModelIndex].push_back(object);
}

std::unordered_map<uint32_t, std::vector<pObject>> ObjectManager::GetObjects() const
{
	return m_objects;
}

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
}
