#pragma once

#include "../Graphics.h"
#include "Texture.h"

#include <memory>
#include <vector>

class TextureManager
{
	friend class Renderer;
	CLASS_SINGLETON_HEADER(TextureManager)
public:
	TextureManager();
	~TextureManager();
	void AddTexture(const pTexture& tex);
	pTexture CreateTexture(const Texture& textureInfo);
	// Registers a texture generated in code. Like every texture, it must be created before the renderer starts.
	pTexture CreateTexture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels);
	pTexture GetTexture(const std::string& textureName) const;
	std::vector<vk::DescriptorImageInfo> CreateTextures();
	uint32_t GetNumberOfTextures();
	uint32_t GetTextureIndex(const std::string textureName);
	std::vector<std::string> GetTextureNames() const;
private:

	std::shared_ptr<Graphics>									m_graphics = nullptr;

	std::vector<pTexture>										m_textures = {};
};