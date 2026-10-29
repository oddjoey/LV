#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Entity.h"
#include "ObjectShading.h"

class Object : public Entity
{
	friend class ObjectManager;
public:
	Object(const glm::vec3& position = {}, const glm::quat& rotation = glm::quat(1.f, 0.f, 0.f, 0.f), const glm::vec3& scale = { 1.f, 1.f, 1.f }, const uint32_t& modelIndex = 0, const uint32_t& textureIndex = 0);
	~Object();

	uint32_t GetIndex() const;

	void SetModel(const uint32_t& index);
	uint32_t GetModelIndex() const;

	void SetTexture(const uint32_t& index);
	uint32_t GetTextureIndex() const;

	// Multiplies the texture and vertex colours.
	Object* SetTint(const glm::vec4& tint);
	glm::vec4 GetTint() const;

	// 0 = lit normally; higher values glow. Used for lamps and hover highlights.
	Object* SetEmissive(const float& emissive);
	float GetEmissive() const;

	Object* SetVisible(const bool& visible);
	bool IsVisible() const;

	// Whether raycasts (e.g. what the player is looking at) can hit this object.
	Object* SetPickable(const bool& pickable);
	bool IsPickable() const;

	Object* SetCastsShadows(const bool& castsShadows);
	bool CastsShadows() const;
	// Whether it shows up in the point lights' shadow maps as well as the sun's (if it casts shadows at
	// all). Off for something whose lamp shadow could never be seen, so moving it doesn't make every
	// lamp near it redraw its map.
	Object* SetCastsLampShadows(const bool& castsLampShadows);
	bool CastsLampShadows() const;

	Object* SetShading(const ObjectShading& shading);
	ObjectShading GetShading() const;

	// A tint alpha below 1 draws the object blended, after everything solid.
	bool IsTransparent() const;
private:
	uint32_t m_index;
	uint32_t m_modelIndex;
	uint32_t m_textureIndex;
	glm::vec4 m_tint = { 1.f, 1.f, 1.f, 1.f };
	float m_emissive = 0.f;
	bool m_visible = true;
	bool m_pickable = false;
	bool m_castsShadows = true;
	bool m_castsLampShadows = true;
	ObjectShading m_shading = ObjectShading::LIT;
};

typedef std::shared_ptr<Object> pObject;