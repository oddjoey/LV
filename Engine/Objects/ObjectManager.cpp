#include "ObjectManager.h"

CLASS_SINGLETON_SOURCE(ObjectManager)

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
