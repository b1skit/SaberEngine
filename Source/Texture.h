// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "NamedObject.h"


namespace re
{
	class Texture final : public virtual en::NamedObject
	{
	public:
		using ImageDataUniquePtr = std::unique_ptr<void, std::function<void(void*)>>;


	public:
		struct PlatformParams : public re::IPlatformParams
		{
			PlatformParams() = default;

			virtual ~PlatformParams() = 0; // API-specific GPU bindings should be destroyed here

			bool m_isCreated = false;
			bool m_isDirty = true; // Signal the platform layer that the texture data has been modified
		};


	public:
		enum Usage : uint8_t
		{
			Color				= 1 << 0,
			ColorTarget			= 1 << 1,
			DepthTarget			= 1 << 2,

			// TODO: Implement support for these:
			StencilTarget		= 1 << 3,
			DepthStencilTarget	= 1 << 4,	

			SwapchainColorProxy	= 1 << 5, // Pre-existing API-provided resource (i.e. backbuffer color target)

			Invalid
		};

		enum class Dimension
		{
			/*Texture1D,*/
			Texture2D,
			/*Texture2DArray,
			Texture3D,*/
			TextureCubeMap,

			Invalid
		};

		enum class Format
		{
			RGBA32F,	// 32 bits per channel x N channels
			RG32F,
			R32F,

			RGBA16F,	// 16 bits per channel x N channels
			RG16F,
			R16F,

			RGBA8,		// 8 bits per channel x N channels
			RG8,
			R8,

			// GPU-only formats:
			Depth32F,

			Invalid
		};

		enum class ColorSpace
		{
			sRGB,
			Linear,

			Invalid
		};


		struct TextureParams
		{
			uint32_t m_width = 4; // Must be a minimum of 4x4 for block compressed formats
			uint32_t m_height = 4;
			uint32_t m_faces = 1;

			Usage m_usage = Usage::Invalid;
			Dimension m_dimension = Dimension::Invalid;
			Format m_format = Format::Invalid;
			ColorSpace m_colorSpace = ColorSpace::Invalid;

			bool m_useMIPs = true; // Should MIPs be created for this texture?
			bool m_addToSceneData = true; // Typically false if the texture is a target
		};


	public:
		static std::shared_ptr<re::Texture> Create(
			std::string const& name, 
			TextureParams const& params,
			bool doFill, 
			glm::vec4 fillColor = glm::vec4(0.f, 0.f, 0.f, 1.f),
			std::vector<ImageDataUniquePtr> initialData = std::vector<ImageDataUniquePtr>());

		~Texture();

		glm::vec4 GetTextureDimenions() const;	// .xyzw = width, height, 1/width, 1/height
		inline uint32_t const& Width() const { return m_texParams.m_width; }
		inline uint32_t const& Height() const { return m_texParams.m_height; }		

		size_t GetTotalBytesPerFace() const;
		void* GetTexelData(uint8_t faceIdx) const;

		uint32_t GetNumMips() const;
		glm::vec4 GetSubresourceDimensions(uint32_t mipLevel) const; // .xyzw = subresource width, height, 1/width, 1/height
		bool IsPowerOfTwo() const;
		bool IsSRGB() const;

		re::Texture::PlatformParams* GetPlatformParams() { return m_platformParams.get(); }
		re::Texture::PlatformParams const* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams);

		TextureParams const& GetTextureParams() const { return m_texParams; }


	public:
		// Static helpers:
		static uint8_t GetNumberOfChannels(const Format texFormat);
		static uint8_t GetNumBytesPerTexel(const Format texFormat);


	private:
		explicit Texture(
			std::string const& name, 
			TextureParams const& params, 
			bool doFill, 
			glm::vec4 const& fillColor, 
			std::vector<ImageDataUniquePtr> initialData = std::vector<ImageDataUniquePtr>());

		void Fill(glm::vec4 solidColor);	// Fill texture with a solid color

		void SetTexel(uint32_t face, uint32_t u, uint32_t v, glm::vec4 value); // u == x == col, v == y == row


	private:
		const TextureParams m_texParams;
		std::unique_ptr<re::Texture::PlatformParams> m_platformParams;

		std::vector<ImageDataUniquePtr> m_initialData; // [1, 6] faces


	private:
		Texture() = delete;
		Texture(Texture const& rhs) = delete;
		Texture(Texture const&& rhs) = delete;
		Texture& operator=(Texture const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline re::Texture::PlatformParams::~PlatformParams() {};
}


