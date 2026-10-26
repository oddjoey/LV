#pragma once

#include <cstdint>
#include <vector>

#include "../Utility/Utility.h"

constexpr int MAX_POINT_LIGHTS = 32;
constexpr int SHADOW_CASCADES = 2;

// Which fragments a point light reaches. Lights aren't shadowed, so a lamp inside a building is
// kept off the ground outside it (and a streetlight off the walls inside) this way instead.
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
	// One enclosed space (a building) gets its own ambient and its own lights.
	bool hasInterior = false;
	glm::vec3 interiorMin = {};
	glm::vec3 interiorMax = {};
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
	glm::vec4 interiorAmbient;	// w: 1 if there is an interior
	glm::vec4 interiorMin;
	glm::vec4 interiorMax;
	glm::vec4 fogColor;			// w: density
	glm::ivec4 lightCount;
	glm::vec4 lightPositionRange[MAX_POINT_LIGHTS];
	glm::vec4 lightColorZone[MAX_POINT_LIGHTS];
	glm::mat4 previousViewProj;	// last frame's, without jitter: for motion vectors
	glm::vec4 renderParams;		// xy: this frame's jitter in clip space, z: texture LOD bias
};
