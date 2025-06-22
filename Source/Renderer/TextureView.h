// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Sampler.h"
#include "Texture.h"

#include "Core/InvPtr.h"

#include "Core/Interfaces/IHashedDataObject.h"


namespace re
{
	struct TextureView : public virtual core::IHashedDataObject
	{	
		const re::Texture::Dimension m_viewDimension; // Also hashed to differentiate union byte patterns


		// Notes:
		// ------
		// re::Texture::k_allMips == -1
		// PlaneSlice: https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources#plane-slice

		typedef struct Texture1DView
		{
			uint32_t m_firstMip = 0;
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} Texture1DView;

		typedef struct Texture1DArrayView
		{
			uint32_t m_firstMip = 0;
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			uint32_t m_firstArraySlice = 0;
			uint32_t m_arraySize = 1;
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} Texture1DArrayView;

		typedef struct Texture2DView
		{
			uint32_t m_firstMip = 0;
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			uint32_t m_planeSlice = 0; // Index in a multi-plane format. SRV/UAV/RTV only
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} Texture2DView;

		typedef struct Texture2DArrayView
		{
			uint32_t m_firstMip = 0;
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			uint32_t m_firstArraySlice = 0;
			uint32_t m_arraySize = 1;
			uint32_t m_planeSlice = 0; // Index in a multi-plane format
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} Texture2DArrayView;

		typedef struct Texture3DView
		{
			uint32_t m_firstMip = 0; // SRV/RTV only
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			float m_resoureceMinLODClamp = 0.f; // SRV only
			uint32_t m_firstWSlice = 0; // UAV only
			uint32_t m_wSize = static_cast<uint32_t>(-1); // // -1: All depth slices from m_firstWSlice on. UAV/RTV only
		} Texture3DView;

		typedef struct TextureCubeView // SRV only
		{
			uint32_t m_firstMip = 0; // SRV only
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} TextureCubeView;

		typedef struct TextureCubeArrayView // SRV only
		{
			uint32_t m_firstMip = 0; // SRV only
			uint32_t m_mipLevels = static_cast<uint32_t>(-1); // -1: All mips from m_firstMip on. SRV only
			uint32_t m_first2DArrayFace; // SRV only
			uint32_t m_numCubes; // SRV only
			float m_resoureceMinLODClamp = 0.f; // SRV only
		} TextureCubeArrayView;

		typedef struct ViewFlags final
		{
			enum DepthFlags : uint8_t
			{
				None					= 0,
				ReadOnlyDepth			= 1 << 0,
				ReadOnlyStencil			= 1 << 1,
				ReadOnlyDepthStencil	= (ReadOnlyDepth | ReadOnlyStencil),
			} m_depthStencil = DepthFlags::None;
		} ViewFlags;

		union
		{
			const Texture1DView Texture1D;
			const Texture1DArrayView Texture1DArray;
			const Texture2DView Texture2D;
			const Texture2DArrayView Texture2DArray;
			const Texture3DView Texture3D;
			const TextureCubeView TextureCube;
			const TextureCubeArrayView TextureCubeArray;
		};

		const ViewFlags Flags;


	public:
		TextureView(TextureView::Texture1DView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::Texture1DArrayView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::Texture2DView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::Texture2DArrayView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::Texture3DView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::TextureCubeView&& view, ViewFlags&& = ViewFlags{});
		TextureView(TextureView::TextureCubeArrayView&& view, ViewFlags&& = ViewFlags{});

		TextureView(core::InvPtr<re::Texture> const& tex, ViewFlags&& = ViewFlags{}); // Create a default view that includes all subresources

		TextureView(/* Don't use this directly */);

		TextureView(TextureView const& rhs) noexcept;
		TextureView(TextureView&& rhs) noexcept;

		TextureView& operator=(TextureView const& rhs) noexcept;
		TextureView& operator=(TextureView&& rhs) noexcept;

		~TextureView() = default;


	public:
		bool DepthWritesEnabled() const;
		bool StencilWritesEnabled() const;
		bool DepthStencilWritesEnabled() const;


	public:
		// For views describing exactly 1 subresource only
		static uint32_t GetSubresourceIndex(core::InvPtr<re::Texture> const&, TextureView const&);

		// Get a subresource index from array/mip indexes RELATIVE to the TextureView's 1st array/mip index
		static uint32_t GetSubresourceIndexFromRelativeOffsets( 
			core::InvPtr<re::Texture> const&, TextureView const&, uint32_t relativeArrayIdx, uint32_t relativeMipIdx);

		// Get a vector of all of the subresources described by a view
		static std::vector<uint32_t> GetSubresourceIndexes(core::InvPtr<re::Texture> const&, re::TextureView const&);

		static void ValidateView(core::InvPtr<re::Texture> const&, re::TextureView const&); // _DEBUG only


	public:
		void ComputeDataHash() override { /* Do nothing: Computed in the ctor*/ }


	private:
		TextureView CreateDefaultView(re::Texture const& tex, ViewFlags&& = ViewFlags{});
	};


	inline bool TextureView::DepthWritesEnabled() const
	{
		return (Flags.m_depthStencil & ViewFlags::DepthFlags::ReadOnlyDepth) == 0;
	}


	inline bool TextureView::StencilWritesEnabled() const
	{
		return (Flags.m_depthStencil & ViewFlags::DepthFlags::ReadOnlyStencil) == 0;
	}


	inline bool TextureView::DepthStencilWritesEnabled() const
	{
		return (Flags.m_depthStencil & ViewFlags::DepthFlags::ReadOnlyDepth) == 0 &&
			(Flags.m_depthStencil & ViewFlags::DepthFlags::ReadOnlyStencil) == 0;
	}


	// -----------------------------------------------------------------------------------------------------------------


	struct TextureAndSamplerInput final
	{
		TextureAndSamplerInput(std::string_view shaderName, core::InvPtr<re::Texture> const&, core::InvPtr<re::Sampler> const&, TextureView const&);

		TextureAndSamplerInput(TextureAndSamplerInput const& rhs) noexcept;
		TextureAndSamplerInput(TextureAndSamplerInput&& rhs) noexcept;

		TextureAndSamplerInput& operator=(TextureAndSamplerInput const& rhs) noexcept;
		TextureAndSamplerInput& operator=(TextureAndSamplerInput&& rhs) noexcept;

		~TextureAndSamplerInput() = default;

		std::string m_shaderName;
		core::InvPtr<re::Texture> m_texture;
		core::InvPtr<re::Sampler> m_sampler;

		TextureView m_textureView;
	};


	struct RWTextureInput final
	{
		RWTextureInput(std::string_view shaderName, core::InvPtr<re::Texture> const&, TextureView const&);

		RWTextureInput(RWTextureInput const& rhs) noexcept;
		RWTextureInput(RWTextureInput&& rhs) noexcept;

		RWTextureInput& operator=(RWTextureInput const& rhs) noexcept;
		RWTextureInput& operator=(RWTextureInput&& rhs) noexcept;

		~RWTextureInput() = default;

		std::string m_shaderName;
		core::InvPtr<re::Texture> m_texture;

		TextureView m_textureView;
	};
}