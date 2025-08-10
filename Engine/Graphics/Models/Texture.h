#pragma once

#include "../Graphics.h"

#include <stb_image.h>

#include <string>

class Texture
{
public:
	Texture(const std::string& name, const std::string& path);
	~Texture();
	uint32_t GetIndex() const;
	void FreeTexture();
	void SetIndex(const uint32_t& index);
private:
	friend class TextureManager;

	std::string				m_name = "";
	std::string				m_path = "";
	uint32_t				m_index = 0;

	int						m_width = 0;
	int						m_height = 0;
	int						m_stride = 0;

	stbi_uc*				m_pixels = nullptr;
	
	vk::Format				m_format = vk::Format::eR8G8B8A8Srgb;
};

typedef std::shared_ptr<Texture> pTexture;