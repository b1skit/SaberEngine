// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasColorTarget())
		{
			return;
		}
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		SEAssert("Color target is already created", !texTargetSetPlatParams->m_colorIsCreated);
		texTargetSetPlatParams->m_colorIsCreated = true;

		const uint8_t numColorTargets = targetSet.GetNumColorTargets();
		SEAssert("Invalid number of color targets", 
			numColorTargets > 0 && numColorTargets <= dx12::SysInfo::GetMaxRenderTargets());

		
		for (re::TextureTarget const& colorTarget : targetSet.GetColorTargets())
		{
			if (!colorTarget.HasTexture())
			{
				continue;
			}

			re::Texture::TextureParams const& texParams = colorTarget.GetTexture()->GetTextureParams();

			// Create RTVs:
			if ((texParams.m_usage & re::Texture::Usage::ColorTarget) || 
				(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy))
			{				
				dx12::Texture::PlatformParams* texPlatParams =
					colorTarget.GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
				
				SEAssert("Texture is not created", texPlatParams->m_isCreated && texPlatParams->m_textureResource);

				// Allocate a descriptor for our view:
				SEAssert("RTV allocations should match the number of faces", 
					texPlatParams->m_rtvDsvDescriptors.size() == texParams.m_faces);
				// TODO: Shouldn't we have an RTV for every subresource? E.g. For rendering into a specific face/mip

				for (uint32_t faceIdx = 0; faceIdx < texParams.m_faces; faceIdx++)
				{
					texPlatParams->m_rtvDsvDescriptors[faceIdx] = std::move(
						context->GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType::RTV).Allocate(1));
					SEAssert("RTV descriptor is not valid", texPlatParams->m_rtvDsvDescriptors[faceIdx].IsValid());
				}				

				re::TextureTarget::TargetParams targetParams = colorTarget.GetTargetParams();

				// Create the RTV:
				D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
				renderTargetViewDesc.Format = texPlatParams->m_format;

				if (texParams.m_faces == 1)
				{
					renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION::D3D12_RTV_DIMENSION_TEXTURE2D;
					renderTargetViewDesc.Texture2D = D3D12_TEX2D_RTV
					{
						.MipSlice = targetParams.m_targetMip,
						.PlaneSlice = targetParams.m_targetFace
					};

					device->CreateRenderTargetView(
						texPlatParams->m_textureResource.Get(), // Pointer to the resource containing the render target texture
						&renderTargetViewDesc,
						texPlatParams->m_rtvDsvDescriptors[0].GetBaseDescriptor()); // Descriptor destination
				}
				else
				{
					SEAssert("We're currently expecting this to be a cubemap",
						texParams.m_faces == 6 && texParams.m_dimension == re::Texture::Dimension::TextureCubeMap);

					renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION::D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

					for (uint32_t faceIdx = 0; faceIdx < texParams.m_faces; faceIdx++)
					{
						// Mip/array slices:
						// https://learn.microsoft.com/en-us/windows/win32/direct3d12/subresources#mip-slice

						renderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
						{
							.MipSlice = 0, // Mip slices include 1 mip level for every texture in an array
							.FirstArraySlice = faceIdx,
							.ArraySize = 1, // Only view one element of our array
							.PlaneSlice = 0 // "Only Plane Slice 0 is valid when creating a view on a non-planar format"
						};

						device->CreateRenderTargetView(
							texPlatParams->m_textureResource.Get(),
							&renderTargetViewDesc,
							texPlatParams->m_rtvDsvDescriptors[faceIdx].GetBaseDescriptor());
					}
				}				
			}
		}
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasDepthTarget())
		{
			return;
		}

		dx12::TextureTargetSet::PlatformParams* targetSetParams = 
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		SEAssert("Target does not have the depth target usage type",
			(targetSet.GetDepthStencilTarget()->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget));

		SEAssert("Depth target is already created", !targetSetParams->m_depthIsCreated);
		targetSetParams->m_depthIsCreated = true;

		std::shared_ptr<re::Texture> depthTargetTex = targetSet.GetDepthStencilTarget()->GetTexture();
		re::Texture::TextureParams const& depthTexParams = depthTargetTex->GetTextureParams();
		dx12::Texture::PlatformParams* depthTexPlatParams =
			depthTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		SEAssert("Depth texture has not been created", 
			depthTexPlatParams->m_isCreated && depthTexPlatParams->m_textureResource);

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = depthTexPlatParams->m_format;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		// Create the depth-stencil descriptor and view:
		for (uint32_t faceIdx = 0; faceIdx < depthTexParams.m_faces; faceIdx++)
		{
			depthTexPlatParams->m_rtvDsvDescriptors[faceIdx] = std::move(
				context->GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType::DSV).Allocate(1));
			SEAssert("DSV descriptor is not valid", depthTexPlatParams->m_rtvDsvDescriptors[faceIdx].IsValid());

			device->CreateDepthStencilView(
				depthTexPlatParams->m_textureResource.Get(),
				&dsv,
				depthTexPlatParams->m_rtvDsvDescriptors[faceIdx].GetBaseDescriptor());
		}
	}


	D3D12_RT_FORMAT_ARRAY TextureTargetSet::GetColorTargetFormats(re::TextureTargetSet const& targetSet)
	{
		// Note: We pack our structure with contiguous DXGI_FORMAT's, regardless of their packing in the 
		// re::TextureTargetSet slots
		D3D12_RT_FORMAT_ARRAY colorTargetFormats{};
		uint32_t numTargets = 0;
		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (!targetSet.GetColorTarget(i).HasTexture())
			{
				break;
			}

			dx12::Texture::PlatformParams const* targetTexPlatParams =
				targetSet.GetColorTarget(i).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			colorTargetFormats.RTFormats[i] = targetTexPlatParams->m_format;
			numTargets++;
		}
		SEAssert("No color targets found", numTargets > 0);
		colorTargetFormats.NumRenderTargets = numTargets;

		return colorTargetFormats;
	}
}