// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "NamedObject.h"


namespace re
{
	class Texture final : public virtual en::NamedObject
	{
	public:
		struct PlatformParams
		{
			PlatformParams() = default;

			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			virtual ~PlatformParams() = 0; // API-specific GPU bindings should be destroyed here

			bool m_isCreated = false;
			bool m_isDirty = true; // Signal the platform layer that the texture data has been modified
		};


	public:
		enum class Usage
		{
			Color,
			ColorTarget,
			DepthTarget,
			/*	StencilTarget,
			DepthStencilTarget,	*/

			Invalid,
			TextureUse_Count = Invalid
		};

		enum class Dimension
		{
			/*Texture1D,*/
			Texture2D,
			/*Texture2DArray,
			Texture3D,*/
			TextureCubeMap,

			Invalid,
			TextureDimension_Count = Invalid
		};

		enum class Format
		{
			RGBA32F,	// 32 bits per channel x N channels
			RGB32F,
			RG32F,
			R32F,

			RGBA16F,	// 16 bits per channel x N channels
			RGB16F,
			RG16F,
			R16F,

			RGBA8,		// 8 bits per channel x N channels
			RGB8,
			RG8,
			R8,

			Depth32F,

			Invalid,
			TextureFormat_Count = Invalid
		};

		enum class ColorSpace
		{
			sRGB,
			Linear,
			Unknown,	// i.e. Texture loaded from disk

			Invalid,
			TextureSpace_Count = Invalid
		};


		struct TextureParams
		{
			uint32_t m_width = 2;
			uint32_t m_height = 2;
			uint32_t m_faces = 1;

			Usage m_usage = Usage::Color;
			Dimension m_dimension = Dimension::Texture2D;
			Format m_format = Format::RGBA32F; // TODO: Default to RGBA8?
			ColorSpace m_colorSpace = ColorSpace::sRGB;

			glm::vec4 m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Also used as initial fill color
			bool m_useMIPs = true; // Should MIPs be created for this texture?
		};


	public:
		explicit Texture(std::string const& name, TextureParams const& params);
		~Texture() { Destroy();	}

		glm::vec4 GetTextureDimenions() const;	// .xyzw = width, height, 1/width, 1/height
		inline uint32_t const& Width() const { return m_texParams.m_width; }
		inline uint32_t const& Height() const { return m_texParams.m_height; }		

		uint8_t const* GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx) const; // u == x == col, v == y == row
		uint8_t const* GetTexel(uint32_t index) const;

		std::vector<uint8_t> const& Texels() const { return m_texels; }
		std::vector<uint8_t>& Texels();

		uint32_t GetNumMips() const;
		uint32_t GetMipDimension(uint32_t mipLevel) const;

		re::Texture::PlatformParams* GetPlatformParams() { return m_platformParams.get(); }
		re::Texture::PlatformParams const* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams);

		void SetTextureParams(re::Texture::TextureParams const& params);
		TextureParams const& GetTextureParams() const { return m_texParams; }


	public:
		// Static helpers:
		static uint8_t GetNumberOfChannels(const Format texFormat);
		static uint8_t GetNumBytesPerTexel(const Format texFormat);


	private:
		void Destroy();

		void Fill(glm::vec4 solidColor);	// Fill texture with a solid color
		void Fill(glm::vec4 tl, glm::vec4 bl, glm::vec4 tr, glm::vec4 br); // Fill texture with a color gradient

		void SetTexel(uint32_t u, uint32_t v, glm::vec4 value); // u == x == col, v == y == row


	private:
		TextureParams m_texParams;
		std::unique_ptr<re::Texture::PlatformParams> m_platformParams;

		std::vector<uint8_t> m_texels;
	

	private:
		Texture() = delete;
		Texture(Texture const& rhs) = delete;
		Texture(Texture const&& rhs) = delete;
		Texture& operator=(Texture const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline re::Texture::PlatformParams::~PlatformParams() {};
}

