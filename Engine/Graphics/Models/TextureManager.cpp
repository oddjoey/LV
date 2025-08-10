#include "TextureManager.h"

std::shared_ptr<TextureManager> TextureManager::s_instance = nullptr;

std::shared_ptr<TextureManager> TextureManager::Get()
{
	if (s_instance == nullptr)
		s_instance = std::shared_ptr<TextureManager>(new TextureManager());

	return s_instance;
}

void TextureManager::AddTexture(const pTexture& tex)
{
	m_textures.push_back(tex);
	tex->m_index = static_cast<uint8_t>(m_textures.size());
}

pTexture TextureManager::CreateTexture(const Texture& textureInfo)
{
	auto texture = std::make_shared<Texture>(textureInfo);
	AddTexture(texture);
	return texture;
}

void TextureManager::CreateTextures()
{
	LOG_INFO("Creating textures...");

	auto graphics = Graphics::Get();
	
	m_textureImages = std::vector<vk::Image>(m_textures.size());
	m_textureImageViews = std::vector<vk::ImageView>(m_textures.size());
	m_textureSamplers = std::vector<vk::Sampler>(m_textures.size());
	m_textureDescriptors = std::vector<vk::DescriptorImageInfo>(m_textures.size());

	int i = 0;
	for (auto& texture : m_textures)
	{
		auto textureSize = static_cast<vk::DeviceSize>(texture->m_width * texture->m_height * texture->m_stride);

		vk::DeviceMemory imageMemory;
		m_textureImages[i] = graphics->CreateImage(texture->m_width, texture->m_height, texture->m_format, vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, imageMemory, true);

		graphics->TransitionImageLayout(m_textureImages[i], texture->m_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		graphics->CopyDataToImage(texture->m_pixels, textureSize, m_textureImages[i], texture->m_width, texture->m_height);
		graphics->TransitionImageLayout(m_textureImages[i], texture->m_format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		 
		texture->FreeTexture();

		m_textureImageViews[i] = graphics->CreateImageView(m_textureImages[i], texture->m_format, vk::ImageAspectFlagBits::eColor, true);

		m_textureSamplers[i] = graphics->CreateSampler(true);

		m_textureDescriptors[i].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		m_textureDescriptors[i].sampler = m_textureSamplers[i];
		m_textureDescriptors[i].imageView = m_textureImageViews[i];
		i++;
	}

	LOG_SUCCESS("Sucessfully created textures!");
}

uint32_t TextureManager::GetNumberOfTextures()
{
	return static_cast<uint32_t>(m_textures.size());
}

uint32_t TextureManager::GetTextureIndex(const std::string textureName)
{
	for (const auto& texture : m_textures)
		if (!texture->m_name.compare(textureName))
			return texture->GetIndex();

	throw std::runtime_error("Texture not found!");
	return 0;
}

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{

}
