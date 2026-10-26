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

struct UniformBuffer
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 previousModel;	// last frame's, for motion vectors
};

// Per-draw values for the fragment shader. Layout must match PushData in shader.frag.
struct ObjectPushConstants
{
	glm::vec4 tint;
	float emissive;
	uint32_t textureIndex;
	uint32_t shading;	// ObjectShading
};

// Objects beyond this many have no uniform buffer slot and are not drawn.
constexpr uint32_t MAX_OBJECTS = 1024;

namespace Utility
{
	// Function to read the contents of a file into memory
	std::vector<char> ReadFile(const std::string& filepath);
}