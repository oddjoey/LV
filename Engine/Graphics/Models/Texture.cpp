#include "Texture.h"

uint32_t Texture::GetIndex() const
{
	return m_index;
}

void Texture::FreeTexture()
{
	stbi_image_free(m_pixels);
}

void Texture::SetIndex(const uint32_t& index)
{
	m_index = index;
}

Texture::Texture(const std::string& name, const std::string& path) : m_name(name), m_path(path)
{
	m_pixels = stbi_load(path.c_str(), &m_width, &m_height, &m_stride, STBI_rgb_alpha);

	if (!m_pixels)
		throw ("Failed to load texture data, wrong path..?");
}

Texture::~Texture()
{
	//stbi_image_free(m_pixels);
}
