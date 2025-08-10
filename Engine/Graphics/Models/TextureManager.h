#pragma once

#include "Texture.h"

#include <memory>
#include <vector>

class TextureManager
{
	friend class Renderer;
public:
	static std::shared_ptr<TextureManager> Get();
	void AddTexture(const pTexture& tex);
	pTexture CreateTexture(const Texture& textureInfo);
	void CreateTextures();
	uint32_t GetNumberOfTextures();
	uint32_t GetTextureIndex(const std::string textureName);
	TextureManager();
	~TextureManager();
private:
	static std::shared_ptr<TextureManager>						s_instance;

	std::vector<pTexture>										m_textures = {};

	std::vector<vk::Image>										m_textureImages = {};
	std::vector<vk::ImageView>									m_textureImageViews = {};
	std::vector<vk::Sampler>									m_textureSamplers = {};
	std::vector<vk::DescriptorImageInfo>						m_textureDescriptors = {};
};