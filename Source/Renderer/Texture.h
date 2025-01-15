// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/InvPtr.h"

#include "Core/Interfaces/ILoadContext.h"
#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformParams.h"
#include "Core/Interfaces/IUniqueID.h"


namespace load
{
	template<typename T>
	struct TextureFromFilePath;
}

namespace
{
	template<typename T>
	struct TextureFromCGLTF;
}

namespace re
{
	class Texture final : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		using ImageDataUniquePtr = std::unique_ptr<void, std::function<void(void*)>>;

		struct IInitialData
		{
			IInitialData(uint32_t arrayDepth, uint8_t numFaces, uint32_t bytesPerFace)
				: m_arrayDepth(arrayDepth), m_numFaces(numFaces), m_bytesPerFace(bytesPerFace)
			{}

			virtual ~IInitialData() = default;

			virtual bool HasData() const = 0;
			virtual uint32_t ArrayDepth() const { return m_arrayDepth; }
			virtual uint8_t NumFaces() const { return m_numFaces; }
			virtual void* GetDataBytes(uint8_t arrayIdx, uint8_t faceIdx) = 0;
			virtual void Clear() = 0;

			uint32_t m_arrayDepth;
			uint8_t m_numFaces;
			uint32_t m_bytesPerFace;

		private:
			IInitialData() = delete;
		};

		struct InitialDataSTBIImage final : public virtual IInitialData
		{
			InitialDataSTBIImage(
				uint32_t arrayDepth, uint8_t numFaces, uint32_t bytesPerFace, std::vector<ImageDataUniquePtr>&& initialData);
			bool HasData() const override;
			void* GetDataBytes(uint8_t arrayIdx, uint8_t faceIdx) override;
			void Clear() override;

			std::vector<ImageDataUniquePtr> m_data; // array elements and faces are packed consecutively
		};

		struct InitialDataVec final : public virtual IInitialData
		{
			InitialDataVec(uint32_t arrayDepth, uint8_t numFaces, uint32_t bytesPerFace, std::vector<uint8_t>&& initialData);
			bool HasData() const override;
			void* GetDataBytes(uint8_t arrayIdx, uint8_t faceIdx) override;
			void Clear() override;

			std::vector<uint8_t> m_data; // Bytes: array [0,N][1, 6] faces
		};


	public:
		static constexpr glm::vec4 k_errorTextureColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);


	public: // Subresource sentinel values:
		static constexpr uint32_t k_allArrayElements	= std::numeric_limits<uint32_t>::max();
		static constexpr uint32_t k_allMips				= std::numeric_limits<uint32_t>::max(); 


	public:
		static uint32_t ComputeMaxMips(uint32_t width, uint32_t height);

		static glm::vec4 ComputeTextureDimenions(uint32_t width, uint32_t height);
		static glm::vec4 ComputeTextureDimenions(glm::uvec2 widthHeight);

		struct TextureParams;
		static uint32_t ComputeTotalBytesPerFace(re::Texture::TextureParams const&, uint32_t mipLevel = 0);

		static void Fill(IInitialData*, TextureParams const&, glm::vec4 const& fillColor);


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual void Destroy() = 0; // API-specific GPU bindings should be destroyed here

			bool m_isCreated = false;
			bool m_isDirty = true; // Signal the platform layer that the texture data has been modified
		};


	public:
		enum Usage : uint8_t
		{
			ColorSrc			= 1 << 0,
			ColorTarget			= 1 << 1,
			DepthTarget			= 1 << 2,

			// TODO: Implement support for these:
			StencilTarget		= 1 << 3,
			DepthStencilTarget	= 1 << 4,	

			SwapchainColorProxy	= 1 << 5, // Pre-existing API-provided resource (i.e. backbuffer color target)

			Invalid = std::numeric_limits<uint8_t>::max()
		};

		enum Dimension : uint8_t
		{
			Texture1D,
			Texture1DArray,
			Texture2D,
			Texture2DArray,
			Texture3D,
			TextureCube,
			TextureCubeArray,

			Dimension_Count,
			Dimension_Invalid = Dimension_Count
		};

		enum class Format
		{
			RGBA32F,	// 32 bits per channel x N channels
			RG32F,
			R32F,

			R32_UINT,

			RGBA16F,	// 16 bits per channel x N channels
			RG16F,
			R16F,

			R16_UNORM,

			RGBA8_UNORM, // 8 bits per channel x N channels
			RG8_UNORM,
			R8_UNORM,

			R8_UINT,

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
			uint32_t m_arraySize = 1; // No. textures in an array texture, or depth slices in a 3D texture

			Usage m_usage = Usage::Invalid;
			Dimension m_dimension = Dimension::Dimension_Invalid;
			Format m_format = Format::Invalid;
			ColorSpace m_colorSpace = ColorSpace::Invalid;

			MipMode m_mipMode = MipMode::AllocateGenerate;
			MultisampleMode m_multisampleMode = MultisampleMode::Disabled;

			bool m_createAsPermanent = false; // Should this texture be kept alive beyond the scope of its InvPtr?

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
		// Create a Texture with data from a vector of bytes. Useful for creating data on the CPU
		[[nodiscard]] static core::InvPtr<re::Texture> Create(
			std::string const& name, TextureParams const& params, std::vector<uint8_t>&& initialData);

		// Create a texture with a solid fill color:
		[[nodiscard]] static core::InvPtr<re::Texture> Create(
			std::string const& name, TextureParams const& params, glm::vec4 fillColor);

		// Create a basic runtime texture (no initial data):
		[[nodiscard]] static core::InvPtr<re::Texture> Create(std::string const& name, TextureParams const& params);

		Texture(Texture&&) noexcept = default;
		Texture& operator=(Texture&&) noexcept = default;

		~Texture();

		void Destroy();

		glm::vec4 GetTextureDimenions() const;	// .xyzw = width, height, 1/width, 1/height
		inline uint32_t const& Width() const { return m_texParams.m_width; }
		inline uint32_t const& Height() const { return m_texParams.m_height; }		

		uint32_t GetTotalBytesPerFace(uint32_t mipLevel = 0) const;

		bool HasInitialData() const;
		void* GetTexelData(uint8_t arrayIdx, uint8_t faceIdx) const; // Can be null
		void ClearTexelData(); // Clear CPU-side texel data

		uint32_t GetNumMips() const;
		glm::vec4 GetMipLevelDimensions(uint32_t mipLevel) const; // .xyzw = subresource width, height, 1/width, 1/height

		uint32_t GetTotalNumSubresources() const; // No. array elements * no. faces * no. of mips
		uint32_t GetSubresourceIndex(uint32_t arrayIdx, uint32_t faceIdx, uint32_t mipIdx) const;

		bool IsPowerOfTwo() const;
		bool IsSRGB() const;

		re::Texture::PlatformParams* GetPlatformParams() { return m_platformParams.get(); }
		re::Texture::PlatformParams const* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<re::Texture::PlatformParams> platformParams);

		TextureParams const& GetTextureParams() const { return m_texParams; }

		static void ShowImGuiWindow(core::InvPtr<re::Texture> const&);


	public:
		// Static helpers:
		static uint8_t GetNumberOfChannels(const Format texFormat);
		
		static uint8_t GetNumBytesPerTexel(const Format texFormat);
		
		static uint8_t GetNumFaces(core::InvPtr<re::Texture> const&);
		static uint8_t GetNumFaces(re::Texture const*);
		static uint8_t GetNumFaces(re::Texture::Dimension);


	protected:
		friend struct load::TextureFromFilePath<re::Texture>;
		friend struct TextureFromCGLTF<re::Texture>;

		explicit Texture(std::string const& name, TextureParams const& params);
		explicit Texture(std::string const& name, TextureParams const& params, std::vector<ImageDataUniquePtr>&&);
		explicit Texture(std::string const& name, TextureParams const& params, std::unique_ptr<InitialDataVec>&&);


	private:
		void Fill(glm::vec4 const& solidColor);	// Fill texture with a solid color

		static void SetTexel( // u == x == col, v == y == row
			IInitialData*,
			TextureParams const&,
			uint8_t arrayIdx, 
			uint32_t faceIdx,
			uint32_t u, 
			uint32_t v, 
			glm::vec4 const& value);
		
		void SetTexel(uint8_t arrayIdx, uint32_t faceIdx, uint32_t u, uint32_t v, glm::vec4 const& value);


	private:
		const TextureParams m_texParams;
		std::unique_ptr<re::Texture::PlatformParams> m_platformParams;

		std::unique_ptr<IInitialData> m_initialData; // Owns a vector with [1,6] faces of data

		const uint32_t m_numMips;
		const uint32_t m_numSubresources; // No. array elements * no. faces * no. of mips


	private:
		Texture() = delete;
		Texture(Texture const&) = delete;
		Texture& operator=(Texture const&) = delete;
	};


	inline uint32_t Texture::GetNumMips() const
	{
		return m_numMips;
	}


	inline uint32_t Texture::GetTotalNumSubresources() const
	{
		return m_numSubresources;
	}
}


