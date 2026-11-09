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
	// clampToEdge: for a texture laid once across its surface (not tiled), so its edges don't blend with the
	// opposite ones.
	pTexture CreateTexture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels, bool clampToEdge);
	// A slot for a code-generated texture that may never be used (a pool, e.g. a wall's paint tiles): it costs
	// nothing at start up, and gets its image the first time UpdateTextures gives it pixels. Until then it
	// samples as another texture, so nothing should draw with it.
	pTexture ReserveTexture(const std::string& name, int width, int height, bool clampToEdge);
	// Replaces code-generated textures' pixels (the same size) once the renderer is running, mips and all,
	// in one go on the GPU without waiting for it (e.g. paint going on a wall). The pixels are copied
	// before it returns.
	void UpdateTexture(uint32_t index, const std::vector<uint8_t>& pixels);
	void UpdateTextures(const std::vector<std::pair<uint32_t, const std::vector<uint8_t>*>>& updates);
	pTexture GetTexture(const std::string& textureName) const;
	std::vector<vk::DescriptorImageInfo> CreateTextures();
	uint32_t GetNumberOfTextures();
	uint32_t GetTextureIndex(const std::string textureName);
	std::vector<std::string> GetTextureNames() const;
private:
	// A reserved texture's image, view and sampler, ready to have pixels uploaded.
	void CreateImage(Texture& texture);

	std::shared_ptr<Graphics>									m_graphics = nullptr;
	vk::Sampler													m_repeatSampler = {};
	vk::Sampler													m_clampSampler = {};

	std::vector<pTexture>										m_textures = {};
};