// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "core/Interfaces/IHashedDataObject.h"


namespace re
{
	class Sampler;


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

		typedef struct ViewFlags
		{
			enum DepthFlags : uint8_t
			{
				None					= 0,
				ReadOnlyDepth			= 1 << 0,
				ReadOnlyStencil			= 1 << 1,
				ReadOnlyDepthStencil	= (ReadOnlyDepth && ReadOnlyStencil),
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
		TextureView(TextureView::Texture1DView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::Texture1DArrayView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::Texture2DView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::Texture2DArrayView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::Texture3DView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::TextureCubeView const& view, ViewFlags const& = ViewFlags{});
		TextureView(TextureView::TextureCubeArrayView const& view, ViewFlags const& = ViewFlags{});

		TextureView(re::Texture const* tex); // Create a default view that includes all subresources
		TextureView(re::Texture const& tex);
		TextureView(std::shared_ptr<re::Texture const> const& tex);

		TextureView(/* Don't use this directly */);

		TextureView(TextureView const& rhs) noexcept;
		TextureView(TextureView&& rhs) noexcept;

		TextureView& operator=(TextureView const& rhs) noexcept;
		TextureView& operator=(TextureView&& rhs) noexcept;


	public:
		bool DepthWritesEnabled() const;
		bool StencilWritesEnabled() const;
		bool DepthStencilWritesEnabled() const;


	public:
		// For views describing exactly 1 subresource only
		static uint32_t GetSubresourceIndex(re::Texture const*, TextureView const&);

		// Get a subresource index from array/mip indexes RELATIVE to the TextureView's 1st array/mip index
		static uint32_t GetSubresourceIndexFromRelativeOffsets( 
			re::Texture const*, TextureView const&, uint32_t relativeArrayIdx, uint32_t relativeMipIdx);

		// Get a vector of all of the subresources described by a view
		static std::vector<uint32_t> GetSubresourceIndexes(re::Texture const*, re::TextureView const&);


	public:
		void ComputeDataHash() override { /* Do nothing: Computed in the ctor*/ }


	private:
		TextureView CreateDefaultView(re::Texture const& tex);
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


	struct TextureAndSamplerInput
	{
		TextureAndSamplerInput(char const* shaderName, re::Texture const*, re::Sampler const*, TextureView const&);
		TextureAndSamplerInput(std::string const& shaderName, re::Texture const*, re::Sampler const*, TextureView const&);

		TextureAndSamplerInput(TextureAndSamplerInput const& rhs) noexcept;
		TextureAndSamplerInput(TextureAndSamplerInput&& rhs) noexcept;

		TextureAndSamplerInput& operator=(TextureAndSamplerInput const& rhs) noexcept;
		TextureAndSamplerInput& operator=(TextureAndSamplerInput&& rhs) noexcept;

		~TextureAndSamplerInput() = default;

		std::string m_shaderName;
		re::Texture const* m_texture;
		re::Sampler const* m_sampler;

		TextureView m_textureView;
	};
}