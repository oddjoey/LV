#include "Texture.h"

uint32_t Texture::GetIndex() const
{
	return m_index;
}

const std::string& Texture::GetName() const
{
	return m_name;
}

const uint8_t* Texture::GetPixelData() const
{
	return m_generatedPixels.empty() ? m_pixels : m_generatedPixels.data();
}

void Texture::FreeTexture()
{
	if (m_pixels)
		stbi_image_free(m_pixels);
	m_pixels = nullptr;
	m_generatedPixels.clear();
	m_generatedPixels.shrink_to_fit();
}

void Texture::SetIndex(const uint32_t& index)
{
	m_index = index;
}

Texture::Texture(const std::string& name, const std::string& path) : m_name(name), m_path(path)
{
	m_pixels = stbi_load(path.c_str(), &m_width, &m_height, &m_stride, STBI_rgb_alpha);
	m_stride++;

	if (!m_pixels)
		throw ("Failed to load texture data, wrong path..?");

	m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
}

Texture::Texture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels)
	: m_name(name), m_width(width), m_height(height), m_stride(4), m_generatedPixels(pixels)
{
	if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * height * 4)
		throw std::runtime_error("Generated texture has the wrong amount of pixel data!");

	m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height)))) + 1;
}

Texture::~Texture()
{
	//stbi_image_free(m_pixels);
}
