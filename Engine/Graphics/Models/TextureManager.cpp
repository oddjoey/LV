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
	tex->m_index = static_cast<uint32_t>(m_textures.size());
}

pTexture TextureManager::CreateTexture(const Texture& textureInfo)
{
	auto texture = std::make_shared<Texture>(textureInfo);
	AddTexture(texture);
	return texture;
}

pTexture TextureManager::CreateTexture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels)
{
	auto texture = std::make_shared<Texture>(name, width, height, pixels);
	AddTexture(texture);
	return texture;
}

pTexture TextureManager::CreateTexture(const std::string& name, int width, int height, const std::vector<uint8_t>& pixels, bool clampToEdge)
{
	auto texture = CreateTexture(name, width, height, pixels);
	texture->m_clampToEdge = clampToEdge;
	return texture;
}

pTexture TextureManager::ReserveTexture(const std::string& name, int width, int height, bool clampToEdge)
{
	if (width <= 0 || height <= 0)
		throw std::runtime_error("A reserved texture needs a size!");
	// A pixel to make it with, dropped straight away: its pixels come with its first update.
	auto texture = std::make_shared<Texture>(name, 1, 1, std::vector<uint8_t>(4, 0));
	texture->FreeTexture();
	texture->m_width = width;
	texture->m_height = height;
	texture->m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
	texture->m_clampToEdge = clampToEdge;
	texture->m_reserved = true;
	AddTexture(texture);
	return texture;
}

void TextureManager::CreateImage(Texture& texture)
{
	vk::DeviceMemory imageMemory;
	texture.m_image = m_graphics->CreateImage(texture.m_width, texture.m_height, texture.m_mipLevels, vk::SampleCountFlagBits::e1,
		texture.m_format, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal, imageMemory, true);
	texture.m_imageView = m_graphics->CreateImageView(texture.m_image, texture.m_format, vk::ImageAspectFlagBits::eColor, texture.m_mipLevels, true);
	texture.m_sampler = texture.m_clampToEdge ? m_clampSampler : m_repeatSampler;
	texture.m_descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	texture.m_descriptor.imageView = texture.m_imageView;
	texture.m_descriptor.sampler = texture.m_sampler;
}

void TextureManager::UpdateTexture(uint32_t index, const std::vector<uint8_t>& pixels)
{
	UpdateTextures({ { index, &pixels } });
}

void TextureManager::UpdateTextures(const std::vector<std::pair<uint32_t, const std::vector<uint8_t>*>>& updates)
{
	std::vector<Graphics::ImageUpload> uploads;
	std::vector<std::pair<uint32_t, vk::DescriptorImageInfo>> newSlots;
	for (const auto& [index, pixels] : updates)
	{
		if (index == 0 || index > m_textures.size() || !pixels)
			continue;
		auto& texture = m_textures[index - 1];
		if (pixels->size() != static_cast<size_t>(texture->m_width * texture->m_height * 4))
			continue;
		// A reserved texture's first pixels: its image now, and its slot pointed at it.
		vk::ImageLayout from = vk::ImageLayout::eShaderReadOnlyOptimal;
		if (!texture->m_image && texture->m_reserved && m_repeatSampler)
		{
			CreateImage(*texture);
			newSlots.push_back({ index, texture->m_descriptor });
			from = vk::ImageLayout::eUndefined;
		}
		if (!texture->m_image)
			continue;
		uploads.push_back({ texture->m_image, static_cast<uint32_t>(texture->m_width), static_cast<uint32_t>(texture->m_height), texture->m_mipLevels,
			pixels->data(), from });
	}
	// The upload's on the queue before any frame that could draw with the new slots.
	m_graphics->WriteTextureDescriptors(newSlots);
	// Called with nothing, it still frees what earlier updates are done with.
	m_graphics->UploadImages(uploads);
}

pTexture TextureManager::GetTexture(const std::string& textureName) const
{
	for (const auto& texture : m_textures)
		if (texture->m_name == textureName)
			return texture;
	return nullptr;
}

std::vector<vk::DescriptorImageInfo> TextureManager::CreateTextures()
{
	LOG_INFO("Creating textures...");

	// The shaders' texture array can't be empty, and reserved slots sample a real texture until they're
	// used, so a game without textures of its own gets one plain white pixel. Nothing refers to it: objects
	// without a texture use index 0.
	if (std::none_of(m_textures.begin(), m_textures.end(), [](const pTexture& texture) { return !texture->m_reserved; }))
		CreateTexture("Lordi Placeholder", 1, 1, { 255, 255, 255, 255 });

	// Every texture's pixels go up in a few batched uploads rather than three waited-on command buffers each:
	// with hundreds of textures, that waiting was most of the start up. A batch is sent once its pixels pass
	// this much, so a set of big textures doesn't need one huge staging buffer.
	constexpr size_t BATCH_BYTES = 64ull * 1024 * 1024;
	std::vector<Graphics::ImageUpload> batch;
	std::vector<pTexture> batched;
	size_t batchBytes = 0;
	auto send = [&]()
	{
		m_graphics->UploadImages(batch);
		// The pixels are in the staging buffer now.
		for (auto& texture : batched)
			texture->FreeTexture();
		batch.clear();
		batched.clear();
		batchBytes = 0;
	};

	// The textures share their two samplers: they all filter alike, and only the wrapping differs.
	m_repeatSampler = m_graphics->CreateSampler(true, false);
	m_clampSampler = m_graphics->CreateSampler(true, true);

	std::vector<vk::DescriptorImageInfo> textureDescriptors(m_textures.size());
	vk::DescriptorImageInfo standIn = {};
	for (auto& texture : m_textures)
	{
		if (texture->m_reserved)
			continue;
		// stide has a chance of being > 4 which causes errors, for now just set to 4..
		auto textureSize = static_cast<size_t>(texture->m_width * texture->m_height * /*texture->m_stride*/ 4);

		CreateImage(*texture);
		if (batchBytes > 0 && batchBytes + textureSize > BATCH_BYTES)
			send();
		batch.push_back({ texture->m_image, static_cast<uint32_t>(texture->m_width), static_cast<uint32_t>(texture->m_height),
			texture->m_mipLevels, texture->GetPixelData(), vk::ImageLayout::eUndefined });
		batched.push_back(texture);
		batchBytes += textureSize;

		textureDescriptors[texture->GetIndex() - 1] = texture->m_descriptor;
		if (!standIn.imageView)
			standIn = texture->m_descriptor;
	}
	if (!batch.empty())
		send();

	// Reserved slots stand in as the first texture until they get images of their own.
	for (auto& texture : m_textures)
		if (texture->m_reserved)
			textureDescriptors[texture->GetIndex() - 1] = standIn;

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
