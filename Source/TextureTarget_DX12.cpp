// © 2022 Adam Badke. All rights reserved.
#include <d3dx12.h>

#include "CastUtils.h"
#include "Config.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

using Microsoft::WRL::ComPtr;


namespace
{
	void CreateViewportAndScissorRect(re::TextureTargetSet const& targetSet)
	{
		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Configure the viewport:
		re::Viewport const& viewport = targetSet.GetViewport();

		texTargetSetPlatParams->m_viewport = CD3DX12_VIEWPORT(
			static_cast<float>(viewport.xMin()),
			static_cast<float>(viewport.yMin()),
			static_cast<float>(viewport.Width()),
			static_cast<float>(viewport.Height()));

		// Configure the scissor rectangle:
		re::ScissorRect const& scissorRect = targetSet.GetScissorRect();

		SEAssert(util::CheckedCast<uint32_t>(scissorRect.Left()) >= targetSet.GetViewport().xMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Top()) >= targetSet.GetViewport().yMin() &&
			util::CheckedCast<uint32_t>(scissorRect.Right()) <= targetSet.GetViewport().Width() &&
			util::CheckedCast<uint32_t>(scissorRect.Bottom()) <= targetSet.GetViewport().Height(),
			"Scissor rectangle is out of bounds of the viewport");

		texTargetSetPlatParams->m_scissorRect = CD3DX12_RECT(
			scissorRect.Left(),
			scissorRect.Top(),
			scissorRect.Right(),
			scissorRect.Bottom());
	}
}

namespace dx12
{
	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasColorTarget())
		{
			return;
		}
		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
		SEAssert(texTargetSetPlatParams->m_isCommitted, "Target set has not been committed");

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();
		
		for (re::TextureTarget const& colorTarget : targetSet.GetColorTargets())
		{
			if (!colorTarget.HasTexture())
			{
				continue;
			}

			dx12::TextureTarget::PlatformParams* targetPlatParams =
				colorTarget.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
			SEAssert(!targetPlatParams->m_isCreated, "Target has already been created");
			targetPlatParams->m_isCreated = true;

			re::Texture::TextureParams const& texParams = colorTarget.GetTexture()->GetTextureParams();

			// Create RTVs:
			if ((texParams.m_usage & re::Texture::Usage::ColorTarget) || 
				(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy))
			{				
				dx12::Texture::PlatformParams* texPlatParams =
					colorTarget.GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
				
				SEAssert(texPlatParams->m_isCreated && texPlatParams->m_textureResource, "Texture is not created");

				re::TextureTarget::TargetParams const& targetParams = colorTarget.GetTargetParams();

				SEAssert(targetPlatParams->m_rtvDsvDescriptors.empty(),
					"RTVs have already been allocated. This is unexpected");

				// Allocate descriptors for our RTVs:
				const uint32_t numFaces = texParams.m_faces;
				const uint32_t numMips = colorTarget.GetTexture()->GetNumMips();

				for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
				{
					for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
					{
						targetPlatParams->m_rtvDsvDescriptors.emplace_back(std::move(
							context->GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType::RTV).Allocate(1)));
						SEAssert(targetPlatParams->m_rtvDsvDescriptors.back().IsValid(), "RTV descriptor is not valid");

						// Create the RTV:
						D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
						renderTargetViewDesc.Format = texPlatParams->m_format;

						switch (numFaces)
						{
						case 1:
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
								targetPlatParams->m_rtvDsvDescriptors.back().GetBaseDescriptor()); // Descriptor destination
						}
						break;
						case 6:
						{
							SEAssert(numFaces == 6 && texParams.m_dimension == re::Texture::Dimension::TextureCubeMap,
								"We're currently expecting this to be a cubemap, but it doesn't need to be");

							renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION::D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

							renderTargetViewDesc.Texture2DArray = D3D12_TEX2D_ARRAY_RTV
							{
								.MipSlice = mipIdx,	// Mip slices include 1 mip level for every texture in an array
								.FirstArraySlice = faceIdx,
								.ArraySize = 1,		// Only view one element of our array
								.PlaneSlice = 0		// "Only Plane Slice 0 is valid when creating a view on a non-planar format"
							};

							device->CreateRenderTargetView(
								texPlatParams->m_textureResource.Get(),
								&renderTargetViewDesc,
								targetPlatParams->m_rtvDsvDescriptors.back().GetBaseDescriptor());
						}
						break;
						default: SEAssertF("Unexpected number of faces");
						}
					}					
				}			
			}
		}

		CreateViewportAndScissorRect(targetSet);
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasDepthTarget())
		{
			return;
		}

		dx12::TextureTargetSet::PlatformParams* targetSetParams = 
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();
		SEAssert(targetSetParams->m_isCommitted, "Target set has not been committed");

		dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
			targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
		SEAssert(!depthTargetPlatParams->m_isCreated, "Target has already been created");
		depthTargetPlatParams->m_isCreated = true;

		std::shared_ptr<re::Texture> depthTargetTex = targetSet.GetDepthStencilTarget()->GetTexture();
		re::Texture::TextureParams const& depthTexParams = depthTargetTex->GetTextureParams();
		SEAssert(depthTexParams.m_usage & re::Texture::Usage::DepthTarget,
			"Target does not have the depth target usage type");

		dx12::Texture::PlatformParams* depthTexPlatParams =
			depthTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
		SEAssert(depthTexPlatParams->m_isCreated && depthTexPlatParams->m_textureResource,
			"Depth texture has not been created");

		// If we don't have any color targets, we must configure the viewport and scissor rect here instead
		if (!targetSet.HasColorTarget())
		{
			CreateViewportAndScissorRect(targetSet);
		}

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		SEAssert(depthTargetPlatParams->m_rtvDsvDescriptors.empty(),
			"DSVs have already been allocated. This is unexpected");

		const uint32_t numFaces = depthTexParams.m_faces;
		const uint32_t numMips = depthTargetTex->GetNumMips();

		SEAssert(numMips == 1, "Depth texture has mips. This is unexpected");

		// Create the depth-stencil descriptor and view:
		for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
		{
			for (uint32_t mipIdx = 0; mipIdx < numMips; mipIdx++)
			{
				depthTargetPlatParams->m_rtvDsvDescriptors.emplace_back(std::move(
					context->GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType::DSV).Allocate(1)));
				SEAssert(depthTargetPlatParams->m_rtvDsvDescriptors.back().IsValid(), "DSV descriptor is not valid");

				D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
				dsv.Format = depthTexPlatParams->m_format;
				dsv.Flags = D3D12_DSV_FLAG_NONE;

				switch (numFaces)
				{
				case 1:
				{
					dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
					dsv.Texture2D.MipSlice = 0;
				}
				break;
				case 6:
				{
					SEAssert(numFaces == 6 && depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMap,
						"We're currently expecting this to be a cubemap");

					dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
					dsv.Texture2DArray.MipSlice = mipIdx; // Mip slices include 1 mip level for every texture in an array
					dsv.Texture2DArray.FirstArraySlice = faceIdx;	// Only view one element of our array
					dsv.Texture2DArray.ArraySize = 1; // Only view one element of our array
				}
				break;
				default: SEAssertF("Unexpected number of faces");
				}

				device->CreateDepthStencilView(
					depthTexPlatParams->m_textureResource.Get(),
					&dsv,
					depthTargetPlatParams->m_rtvDsvDescriptors.back().GetBaseDescriptor());
			}
		}

		// Create a DSV for all cube map faces at once
		if (depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMap)
		{
			depthTargetPlatParams->m_cubemapDescriptor = 
				std::move(context->GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType::DSV).Allocate(1));
			SEAssert(depthTargetPlatParams->m_cubemapDescriptor.IsValid(), "Cube DSV descriptor is not valid");

			D3D12_DEPTH_STENCIL_VIEW_DESC cubeDsv = {};
			cubeDsv.Format = depthTexPlatParams->m_format;
			cubeDsv.Flags = D3D12_DSV_FLAG_NONE;
			cubeDsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			cubeDsv.Texture2DArray.MipSlice = 0; // Mip slices include 1 mip level for every texture in an array
			cubeDsv.Texture2DArray.FirstArraySlice = 0;	// Only view one element of our array
			cubeDsv.Texture2DArray.ArraySize = 6; // Only view one element of our array

			device->CreateDepthStencilView(
				depthTexPlatParams->m_textureResource.Get(),
				&cubeDsv,
				depthTargetPlatParams->m_cubemapDescriptor.GetBaseDescriptor());
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
		SEAssert(numTargets > 0, "No color targets found");
		colorTargetFormats.NumRenderTargets = numTargets;

		return colorTargetFormats;
	}
}