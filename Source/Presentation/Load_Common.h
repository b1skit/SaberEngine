// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class Inventory;
}

namespace load
{
	template<typename T>
	struct TextureFromFilePath : public virtual core::ILoadContext<T>
	{
		void OnLoadBegin(core::InvPtr<re::Texture>&) override;
		std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>&) override;

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


	struct IBLTextureFromFilePath final : public virtual TextureFromFilePath<re::Texture>
	{
		// We override this so we can skip the early registration (which would make the render thread wait)
		void OnLoadBegin(core::InvPtr<re::Texture>&) override;
		std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>& newIBL) override;
		void OnLoadComplete(core::InvPtr<re::Texture>& newIBL) override;

		enum class ActivationMode
		{
			Always,
			IfNoneExists, // If no Ambient IBL exists when we're creating this one, make it active (E.g. Scene default)
			Never,
		};
		ActivationMode m_activationMode = ActivationMode::Always;
	};


	core::InvPtr<re::Texture> ImportIBL(
		core::Inventory* inventory,
		std::string const& filepath,
		IBLTextureFromFilePath::ActivationMode activationMode,
		bool makePermanent = false);
}