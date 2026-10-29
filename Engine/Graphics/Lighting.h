#pragma once

#include <cstdint>
#include <vector>

#include "../Utility/Utility.h"

constexpr int MAX_POINT_LIGHTS = 32;
constexpr int SHADOW_CASCADES = 2;
// Point lights that cast shadows get one shadow map each, up to this many per frame.
constexpr int MAX_SHADOWED_LIGHTS = 8;
// Enclosed spaces (buildings) with their own ambient and lights.
constexpr int MAX_INTERIORS = 4;

// Which fragments a point light reaches. Most lights aren't shadowed, so a lamp inside a building
// is kept off the ground outside it (and a streetlight off the walls inside) this way instead.
enum class LightZone : int
{
	EVERYWHERE = 0,
	INTERIOR = 1,
	EXTERIOR = 2
};

struct PointLight
{
	glm::vec3 position = {};
	float range = 6.f;
	glm::vec3 color = { 1.f, 1.f, 1.f };	// premultiplied by intensity
	LightZone zone = LightZone::EVERYWHERE;
	// A shadowed light sees the scene through one wide shadow map aimed along shadowDirection
	// (straight down suits a ceiling lamp); outside that cone it lights without shadows. Only the
	// first MAX_SHADOWED_LIGHTS that ask for shadows get them.
	bool castsShadows = false;
	glm::vec3 shadowDirection = { 0.f, -1.f, 0.f };
	float shadowConeDegrees = 150.f;
};

// Everything the scene is lit by. The game sets this every frame through the Renderer.
struct SceneLighting
{
	glm::vec3 sunDirection = glm::normalize(glm::vec3(0.35f, 1.f, 0.25f));	// towards the sun
	glm::vec3 sunColor = { 2.2f, 2.05f, 1.85f };
	// Outdoor ambient: sky light from above, bounce from the ground below.
	glm::vec3 skyZenith = { 0.25f, 0.42f, 0.75f };
	glm::vec3 skyHorizon = { 0.65f, 0.74f, 0.85f };
	glm::vec3 groundColor = { 0.20f, 0.19f, 0.16f };
	// Enclosed spaces (a garage, a house) get their own ambient and their own lights. Up to
	// MAX_INTERIORS boxes; a fragment in any of them is inside.
	struct Interior
	{
		glm::vec3 min = {};
		glm::vec3 max = {};
	};
	std::vector<Interior> interiors = {};
	glm::vec3 interiorAmbient = { 0.18f, 0.18f, 0.19f };
	glm::vec3 fogColor = { 0.65f, 0.74f, 0.85f };
	float fogDensity = 0.f;		// per metre
	float exposure = 1.f;
	std::vector<PointLight> pointLights = {};
};

struct ShadowSettings
{
	uint32_t resolution = 2048;	// per cascade; 0 turns sun shadows off
	float distance = 40.f;		// metres from the camera that still get shadows
};

// The per-frame scene uniform block. Layout must match SceneData in the shaders (std140).
struct SceneUniforms
{
	glm::mat4 lightViewProj[SHADOW_CASCADES];
	glm::vec4 shadowParams;		// xy: world size of a shadow texel per cascade, z: shadows on, w: 1 / resolution
	glm::vec4 cameraPosition;	// w: exposure
	glm::vec4 sunDirection;
	glm::vec4 sunColor;
	glm::vec4 skyZenith;
	glm::vec4 skyHorizon;
	glm::vec4 groundColor;
	glm::vec4 interiorAmbient;	// w: how many interiors there are
	glm::vec4 interiorMin[MAX_INTERIORS];
	glm::vec4 interiorMax[MAX_INTERIORS];
	glm::vec4 fogColor;			// w: density
	glm::ivec4 lightCount;
	glm::vec4 lightPositionRange[MAX_POINT_LIGHTS];
	glm::vec4 lightColorZone[MAX_POINT_LIGHTS];
	glm::mat4 previousViewProj;	// last frame's, without jitter: for motion vectors
	glm::vec4 renderParams;		// xy: this frame's jitter in clip space, z: texture LOD bias
	glm::mat4 lightShadowViewProj[MAX_SHADOWED_LIGHTS];
	glm::vec4 lightShadow[MAX_POINT_LIGHTS];	// x: shadow map layer or -1, y: shadow texel size per metre of distance, z: 1 / resolution
	glm::mat4 view;
	glm::mat4 proj;				// with this frame's jitter
};
