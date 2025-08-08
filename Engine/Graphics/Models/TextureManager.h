#pragma once

#include "Texture.h"

#include <memory>
#include <vector>

class TextureManager
{
	friend class Renderer;
public:
	static std::shared_ptr<TextureManager> Get();
	void AddTexture(pTexture tex);
	pTexture CreateTexture(Texture textureInfo);
	void CreateTextures();
	uint32_t GetNumberOfTextures();
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