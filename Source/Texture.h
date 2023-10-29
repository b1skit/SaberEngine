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
		static constexpr uint32_t k_allMips = std::numeric_limits<uint32_t>::max(); // Mip sentinel value


	public:
		static uint32_t ComputeMaxMips(uint32_t width, uint32_t height);


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
			Color				= 1 << 0, // TODO: Rename this "Source" or "Input" or similar
			ColorTarget			= 1 << 1,
			ComputeTarget		= 1 << 2,
			DepthTarget			= 1 << 3,

			// TODO: Implement support for these:
			StencilTarget		= 1 << 4,
			DepthStencilTarget	= 1 << 5,	

			SwapchainColorProxy	= 1 << 6, // Pre-existing API-provided resource (i.e. backbuffer color target)

			Invalid
		};

		enum Dimension : uint8_t
		{
			//Texture1D,
			Texture2D,
			Texture2DArray,
			//Texture3D,
			TextureCubeMap,

			Dimension_Count,
			Dimension_Invalid = Dimension_Count
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

		enum class MipMode : uint8_t
		{
			None,				// Mips are disabled for this texture
			Allocate,			// Mips will be allocated for this texture, but not generated
			AllocateGenerate	// Mips will be both allocated and generated for this texture
		};

		enum class MultisampleMode : bool
		{
			Disabled = false,
			Enabled = true
		};

		struct TextureParams
		{
			uint32_t m_width = 4; // Must be a minimum of 4x4 for block compressed formats
			uint32_t m_height = 4;
			uint32_t m_faces = 1;

			Usage m_usage = Usage::Invalid; // TODO: This should be an uint8_t
			Dimension m_dimension = Dimension::Dimension_Invalid;
			Format m_format = Format::Invalid;
			ColorSpace m_colorSpace = ColorSpace::Invalid;

			MipMode m_mipMode = MipMode::AllocateGenerate;
			MultisampleMode m_multisampleMode = MultisampleMode::Disabled;

			bool m_addToSceneData = true; // Typically false if the texture is a target

			union ClearValues
			{
				glm::vec4 m_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

				struct
				{
					float m_depth = 1.f;
					uint8_t m_stencil = 0;
				} m_depthStencil;

				ClearValues() { memset(this, 0, sizeof(ClearValues)); }
			} m_clear;
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

		bool HasInitialData() const;
		void* GetTexelData(uint8_t faceIdx) const; // Can be null
		void ClearTexelData(); // Clear CPU-side texel data

		uint32_t GetNumMips() const;
		uint32_t GetTotalNumSubresources() const;
		glm::vec4 GetSubresourceDimensions(uint32_t mipLevel) const; // .xyzw = subresource width, height, 1/width, 1/height
		bool IsPowerOfTwo() const;
		bool IsSRGB() const;

		re::Texture::PlatformParams* GetPlatformParams() { return m_platformParams.get(); }
		re::Texture::PlatformParams const* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams);

		TextureParams const& GetTextureParams() const { return m_texParams; }

		void ShowImGuiWindow();


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

		const uint32_t m_numMips;
		const uint32_t m_numSubresources;


	private:
		Texture() = delete;
		Texture(Texture const& rhs) = delete;
		Texture(Texture const&& rhs) = delete;
		Texture& operator=(Texture const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline re::Texture::PlatformParams::~PlatformParams() {};


	inline uint32_t Texture::GetNumMips() const
	{
		return m_numMips;
	}


	inline uint32_t Texture::GetTotalNumSubresources() const
	{
		return m_numSubresources;
	}
}


