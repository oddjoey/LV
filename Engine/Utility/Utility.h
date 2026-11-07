#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define CLASS_SINGLETON_HEADER(type)								\
		static std::shared_ptr<type> s_instance;					\
	public:															\
		static std::shared_ptr<type> Get();

#define CLASS_SINGLETON_SOURCE(type)								\
		std::shared_ptr<type> type::s_instance = nullptr;			\
		std::shared_ptr<type> type::Get()							\
		{															\
			if (s_instance == nullptr)								\
				s_instance = std::shared_ptr<type>(new type());		\
			return s_instance;										\
		}

// One object's slot in the instance buffer the scene shaders read (std430): where it is now and was
// last frame, and how it's shaded. Layout must match InstanceData in instance_data.glsl.
struct InstanceData
{
	glm::mat4 model;
	glm::mat4 previousModel;	// last frame's, for motion vectors
	glm::vec4 tint;
	float emissive;
	uint32_t textureIndex;
	uint32_t shading;			// ObjectShading
	uint32_t flags;				// INSTANCE_IN_FRONT
};
// InstanceData::flags: drawn in front of everything else (Object::SetInFront).
constexpr uint32_t INSTANCE_IN_FRONT = 1u;
static_assert(sizeof(InstanceData) == 160, "InstanceData must match the shaders' std430 layout.");

// Objects beyond this many have no instance slot and are not drawn.
constexpr uint32_t MAX_OBJECTS = 8192;
// How many slots the frame's passes can list between them: the main pass and every shadow map. A
// draw instances a run of these, one per object of the same model.
constexpr uint32_t MAX_DRAW_ENTRIES = MAX_OBJECTS * 16;

namespace Utility
{
	// Function to read the contents of a file into memory
	std::vector<char> ReadFile(const std::string& filepath);
}