// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class TextureTargetSet;


	class TextureTarget
	{
	public:
		struct PlatformParams final : public re::TextureTarget::PlatformParams
		{
			// Individual subresource descriptors: Target a single subresource by array element, face, and mip level
			// Subresource index = (arrayIdx * No. faces * No. mips) + (faceIdx * No. mips) + mipIdx
			dx12::DescriptorAllocation m_subresourceDescriptors;

			// TextureCubeMap, TextureCubeMapArray: Descriptors for viewing an array of cubemap resources as individual
			// cubemaps, by mip level
			// Subresource index = (arrayIdx * numMips) + mipIdx
			dx12::DescriptorAllocation m_cubemapDescriptors;
		};


	public:
		static D3D12_CPU_DESCRIPTOR_HANDLE GetTargetDescriptor(re::TextureTarget const&);


	private: // Helpers:
		static uint32_t GetTargetDescriptorIndex(
			re::Texture const*, uint32_t arrayIdx, uint32_t faceIdx, uint32_t mipIdx);

		static uint32_t GetNumRequiredCubemapTargetDescriptors(re::Texture const*);


	private:
		friend class dx12::TextureTargetSet;
	};


	class TextureTargetSet
	{
	public:
		struct PlatformParams final : public re::TextureTargetSet::PlatformParams
		{
			D3D12_VIEWPORT m_viewport;
			D3D12_RECT m_scissorRect;
		};


	public:
		static void CreateColorTargets(re::TextureTargetSet const& targetSet);
		static void CreateDepthStencilTarget(re::TextureTargetSet const& targetSet);

		static D3D12_RT_FORMAT_ARRAY GetColorTargetFormats(re::TextureTargetSet const& targetSet);
	};
}