// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

struct cgltf_material;


namespace util
{
	re::Texture::ImageDataUniquePtr CreateImageDataUniquePtr(void* imageData);


	std::shared_ptr<re::Texture> LoadTextureFromFilePath(
		std::vector<std::string> texturePaths,
		bool returnErrorTex,
		glm::vec4 const& errorTexFillColor,
		re::Texture::ColorSpace colorSpace);


	std::shared_ptr<re::Texture> LoadTextureFromMemory(
		std::string const& texName,
		unsigned char const* texSrc,
		uint32_t texSrcNumBytes,
		re::Texture::ColorSpace colorSpace);


	std::string GenerateTextureColorFallbackName(
		glm::vec4 const& colorFallback, size_t numChannels, re::Texture::ColorSpace colorSpace);


	std::string GenerateEmbeddedTextureName(char const* texName);


	std::string GenerateMaterialName(cgltf_material const& material);
}