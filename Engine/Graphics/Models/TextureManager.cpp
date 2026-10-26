#include "TextureManager.h"

CLASS_SINGLETON_SOURCE(TextureManager)

TextureManager::TextureManager()
{
	m_graphics = Graphics::Get();
}

TextureManager::~TextureManager()
{

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

std::vector<vk::DescriptorImageInfo> TextureManager::CreateTextures()
{
	LOG_INFO("Creating textures...");

	std::vector<vk::DescriptorImageInfo> textureDescriptors(m_textures.size());
	for (auto& texture : m_textures)
	{
		// stide has a chance of being > 4 which causes errors, for now just set to 4..
		auto textureSize = static_cast<vk::DeviceSize>(texture->m_width * texture->m_height * /*texture->m_stride*/ 4);

		vk::DeviceMemory imageMemory;
		texture->m_image = m_graphics->CreateImage(texture->m_width, texture->m_height, texture->m_mipLevels, vk::SampleCountFlagBits::e1,
			texture->m_format, vk::ImageTiling::eOptimal, 
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal, imageMemory, true);

		m_graphics->TransitionImageLayout(texture->m_image, texture->m_format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, texture->m_mipLevels);
		m_graphics->CopyDataToImage(texture->m_pixels, textureSize, texture->m_image, texture->m_width, texture->m_height);
		m_graphics->GenerateMipmaps(texture->m_image, texture->m_width, texture->m_height, texture->m_mipLevels);
		texture->FreeTexture();

		texture->m_imageView = m_graphics->CreateImageView(texture->m_image, texture->m_format, vk::ImageAspectFlagBits::eColor, texture->m_mipLevels, true);

		texture->m_sampler = m_graphics->CreateSampler(true);

		texture->m_descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		texture->m_descriptor.imageView = texture->m_imageView;
		texture->m_descriptor.sampler = texture->m_sampler;
		
		textureDescriptors[texture->GetIndex() - 1] = texture->m_descriptor;
	}

	LOG_SUCCESS("Sucessfully created textures!");
	return textureDescriptors;
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

std::vector<std::string> TextureManager::GetTextureNames() const
{
	std::vector<std::string> names(m_textures.size() + 1);
	names[0] = "None";
	for (uint32_t i = 0; i < m_textures.size(); i++)
		names[i + 1] = m_textures[i]->m_name;
	return names;
}
