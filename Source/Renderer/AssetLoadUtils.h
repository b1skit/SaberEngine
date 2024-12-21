// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

struct cgltf_material;


namespace grutil
{
	re::Texture::ImageDataUniquePtr CreateImageDataUniquePtr(void* imageData);


	template<typename T>
	struct TextureFromFilePath final : public virtual core::ILoadContext<T>
	{
		void OnLoadBegin(core::InvPtr<re::Texture>) override;
		std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>) override;

		std::string m_filePath;

		glm::vec4 m_colorFallback = re::Texture::k_errorTextureColor;
		re::Texture::Format m_formatFallback = re::Texture::Format::RGBA8_UNORM;
		re::Texture::ColorSpace m_colorSpace = re::Texture::ColorSpace::sRGB;
		re::Texture::MipMode m_mipMode = re::Texture::MipMode::None;
	};


	bool LoadTextureDataFromFilePath( // Returns true if load was successful, or false otherwise
		re::Texture::TextureParams& texParamsOut,
		std::vector<re::Texture::ImageDataUniquePtr>& imageDataOut,
		std::vector<std::string> const& texturePaths,
		std::string const& idName,
		re::Texture::ColorSpace colorSpace,
		bool returnErrorTex,
		bool createAsPermanent = false,
		glm::vec4 const& errorTexFillColor = glm::vec4(1.f, 0.f, 1.f, 1.f));


	bool LoadTextureDataFromMemory( // Returns true if load was successful, or false otherwise
		re::Texture::TextureParams& texParamsOut,
		std::vector<re::Texture::ImageDataUniquePtr>& imageDataOut,
		std::string const& texName,
		unsigned char const* texSrc,
		uint32_t texSrcNumBytes,
		re::Texture::ColorSpace colorSpace);


	std::string GenerateTextureColorFallbackName(
		glm::vec4 const& colorFallback, size_t numChannels, re::Texture::ColorSpace colorSpace);


	std::string GenerateMaterialName(cgltf_material const& material);
}