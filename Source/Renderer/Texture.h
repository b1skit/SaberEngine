// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Renderer/Shaders/Common/ResourceCommon.h"


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
		static glm::vec4 ComputeTextureDimenions(uint32_t width, uint32_t height);
		static glm::vec4 ComputeTextureDimenions(glm::uvec2 widthHeight);

		struct TextureParams;
		static uint32_t ComputeTotalBytesPerFace(re::Texture::TextureParams const&, uint32_t mipLevel = 0);

		static void Fill(IInitialData*, TextureParams const&, glm::vec4 const& fillColor);


	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual void Destroy() override = 0; // API-specific GPU bindings should be destroyed here

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

		enum class Format : uint8_t
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
		static constexpr bool IsCompatibleGroupFormat(Format, Format);

		enum class ColorSpace : uint8_t
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

		struct TextureParams final
		{
			uint32_t m_width = 4; // Must be a minimum of 4x4 for block compressed formats
			uint32_t m_height = 4;
			uint32_t m_arraySize = 1; // No. textures in an array texture, or depth slices in a 3D texture
			uint32_t m_numMips = k_allMips; // k_allMips = Max. mips possible. Otherwise [1, log2(max(width, height)) + 1]

			Usage m_usage = Usage::Invalid;
			Dimension m_dimension = Dimension::Dimension_Invalid;
			Format m_format = Format::Invalid;
			ColorSpace m_colorSpace = ColorSpace::Invalid;

			MipMode m_mipMode = MipMode::AllocateGenerate;
			MultisampleMode m_multisampleMode = MultisampleMode::Disabled;

			bool m_createAsPermanent = false; // Should this texture be kept alive beyond the scope of its InvPtr?

			// Optimized clear values: Choose the value that clear operations will be most commonly called with
			// Note: No effect for OpenGL
			union OptimizedClearVals
			{
				glm::vec4 m_color;

				struct
				{
					float m_depth;
					uint8_t m_stencil;
				} m_depthStencil;

				OptimizedClearVals() { memset(this, 0, sizeof(OptimizedClearVals)); } // Zero-initialized
			} m_optimizedClear;
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
		uint32_t const& Width() const;
		uint32_t const& Height() const;

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

		re::Texture::PlatObj* GetPlatformObject() const;
		void SetPlatformObject(std::unique_ptr<re::Texture::PlatObj> platObj);

		TextureParams const& GetTextureParams() const;

		bool HasUsageBit(Usage) const;

		ResourceHandle GetBindlessResourceHandle(re::ViewType) const;

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

		// Load context helper:
		static void RegisterBindlessResourceHandles(re::Texture* tex, core::InvPtr<re::Texture> const& loadingTexPtr);

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
		std::unique_ptr<re::Texture::PlatObj> m_platObj;

		std::unique_ptr<IInitialData> m_initialData; // Owns a vector with [1,6] faces of data

		const uint32_t m_numMips; // No. of actual mip levels (computed from TextureParams::m_numMips)
		const uint32_t m_numSubresources; // No. array elements * no. faces * no. of mips

		ResourceHandle m_srvResourceHandle;
		ResourceHandle m_uavResourceHandle;


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


	inline uint32_t const& re::Texture::Width() const
	{
		return m_texParams.m_width;
	}


	inline uint32_t const& re::Texture::Height() const
	{
		return m_texParams.m_height;
	}


	inline bool Texture::IsSRGB() const
	{
		return m_texParams.m_colorSpace == ColorSpace::sRGB;
	}


	inline re::Texture::PlatObj* re::Texture::GetPlatformObject() const
	{
		return m_platObj.get();
	}


	inline re::Texture::TextureParams const& re::Texture::GetTextureParams() const
	{
		return m_texParams;
	}


	inline bool Texture::HasUsageBit(Texture::Usage usage) const
	{
		return m_texParams.m_usage & usage;
	}


	inline ResourceHandle Texture::GetBindlessResourceHandle(re::ViewType viewType) const
	{
		switch (viewType)
		{
		case re::ViewType::SRV:
		{
			return m_srvResourceHandle;
		}
		break;
		case re::ViewType::UAV:
		{
			return m_uavResourceHandle;
		}
		break;
		case re::ViewType::CBV:
		default: SEAssertF("Invalid view type");
		}
		return INVALID_RESOURCE_IDX; // This should never happen
	}


	inline constexpr bool Texture::IsCompatibleGroupFormat(re::Texture::Format a, re::Texture::Format b)
	{
		// Return true if the formats are from the same type group
		switch (a)
		{
		case re::Texture::Format::RGBA32F:
		{
			return b == re::Texture::Format::RGBA32F;
		}
		break;
		case re::Texture::Format::RG32F:
		{
			return b == re::Texture::Format::RG32F;
		}
		break;
		case re::Texture::Format::R32F:
		case re::Texture::Format::R32_UINT:
		{
			return b == re::Texture::Format::R32F || 
				b == re::Texture::Format::R32_UINT;
		}
		break;
		case re::Texture::Format::RGBA16F:
		{
			return b == re::Texture::Format::RGBA16F;
		}
		break;
		case re::Texture::Format::RG16F:
		{
			return b == re::Texture::Format::RG16F;
		}
		break;
		case re::Texture::Format::R16F:
		case re::Texture::Format::R16_UNORM:
		{
			return b == re::Texture::Format::R16F ||
				b == re::Texture::Format::R16_UNORM;
		}
		break;
		case re::Texture::Format::RGBA8_UNORM:
		{
			return b == re::Texture::Format::RGBA8_UNORM;
		}
		break;
		case re::Texture::Format::RG8_UNORM:
		{
			return b == re::Texture::Format::RG8_UNORM;
		}
		break;
		case re::Texture::Format::R8_UNORM:
		case re::Texture::Format::R8_UINT:
		{
			return b == re::Texture::Format::R8_UNORM ||
				b == re::Texture::Format::R8_UINT;
		}
		break;
		case re::Texture::Format::Depth32F:
		{
			return b == re::Texture::Format::Depth32F;
		}
		break;
		case re::Texture::Format::Invalid:
		default: return false; // This should never happen
		}
		SEStaticAssert(static_cast<uint32_t>(re::Texture::Format::Invalid) == 13,
			"Number of texture formats changed, this must be updated");
	}
}


