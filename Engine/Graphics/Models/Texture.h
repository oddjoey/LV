#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <stb_image.h>

#include <string>
#include <vector>

class Texture
{
public:
	// Loads an image file.
	Texture(const std::string& name, const std::string& path);
	// Wraps RGBA8 pixels generated in code; pixels.size() must be width * height * 4.
	Texture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels);
	~Texture();
	uint32_t GetIndex() const;
	const std::string& GetName() const;
	void FreeTexture();
	void SetIndex(const uint32_t& index);
private:
	friend class TextureManager;

	const uint8_t* GetPixelData() const;

	std::string					m_name = "";
	std::string					m_path = "";
	uint32_t					m_index = 0;

	int							m_width = 0;
	int							m_height = 0;
	int							m_stride = 0;

	stbi_uc*					m_pixels = nullptr;
	std::vector<uint8_t>		m_generatedPixels = {};
	
	vk::Format					m_format = vk::Format::eR8G8B8A8Srgb;

	vk::Image					m_image = {};
	vk::ImageView				m_imageView = {};
	vk::Sampler					m_sampler = {};
	vk::DescriptorImageInfo		m_descriptor = {};

	uint32_t					m_mipLevels = 0;
	bool						m_clampToEdge = false;	// sampled clamped at its edges rather than repeating
	bool						m_reserved = false;		// no image until its first pixels arrive (TextureManager::ReserveTexture)
};

typedef std::shared_ptr<Texture> pTexture;