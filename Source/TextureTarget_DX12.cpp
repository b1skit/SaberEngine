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

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		dx12::TextureTargetSet::PlatformParams* texTargetSetPlatParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		SEAssert("Color target is already created", !texTargetSetPlatParams->m_colorIsCreated);
		texTargetSetPlatParams->m_colorIsCreated = true;

		const uint8_t numColorTargets = targetSet.GetNumColorTargets();
		SEAssert("Invalid number of color targets", 
			numColorTargets > 0 && numColorTargets <= dx12::SysInfo::GetMaxRenderTargets());

		
		for (std::unique_ptr<re::TextureTarget> const& colorTarget : targetSet.GetColorTargets())
		{
			if (colorTarget == nullptr)
			{
				continue;
			}

			re::Texture::TextureParams const& texParams = colorTarget->GetTexture()->GetTextureParams();

			// Create RTVs:
			if ((texParams.m_usage & re::Texture::Usage::ColorTarget) || 
				(texParams.m_usage & re::Texture::Usage::SwapchainColorProxy))
			{				
				dx12::Texture::PlatformParams* texPlatParams =
					colorTarget->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
				
				SEAssert("Texture is not created", texPlatParams->m_isCreated && texPlatParams->m_textureResource);

				dx12::TextureTarget::PlatformParams* targetPlatParams = 
					colorTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

				// Allocate a descriptor for our view:
				targetPlatParams->m_rtvDsvDescriptor = std::move(
					ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::RTV].Allocate(1));

				re::TextureTarget::TargetParams targetParams = colorTarget->GetTargetParams();

				// Create the RTV:
				D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
				renderTargetViewDesc.Format = texPlatParams->m_format;
				
				SEAssert("TODO: Support render targets of different dimensions",
					texParams.m_dimension == re::Texture::Dimension::Texture2D && texParams.m_faces == 1);

				renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION::D3D12_RTV_DIMENSION_TEXTURE2D;
				renderTargetViewDesc.Texture2D = D3D12_TEX2D_RTV
				{
					.MipSlice = targetParams.m_targetSubesource,
					.PlaneSlice = targetParams.m_targetFace
				};

				device->CreateRenderTargetView(
					texPlatParams->m_textureResource.Get(), // Pointer to the resource containing the render target texture
					&renderTargetViewDesc,
					targetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor()); // Descriptor destination
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
		dx12::Texture::PlatformParams* depthTexPlatParams =
			depthTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		SEAssert("Depth texture has not been created", 
			depthTexPlatParams->m_isCreated && depthTexPlatParams->m_textureResource);

		dx12::TextureTarget::PlatformParams* targetPlatParams =
			targetSet.GetDepthStencilTarget()->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// Create the depth-stencil descriptor and view:
		targetPlatParams->m_rtvDsvDescriptor = std::move(
			ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::DSV].Allocate(1));
		SEAssert("DSV descriptor is not valid", targetPlatParams->m_rtvDsvDescriptor.IsValid());

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = depthTexPlatParams->m_format;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();
		device->CreateDepthStencilView(
			depthTexPlatParams->m_textureResource.Get(),
			&dsv,
			targetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor());
	}


	D3D12_RT_FORMAT_ARRAY TextureTargetSet::GetColorTargetFormats(re::TextureTargetSet const& targetSet)
	{
		// Note: We pack our structure with contiguous DXGI_FORMAT's, regardless of their packing in the 
		// re::TextureTargetSet slots
		D3D12_RT_FORMAT_ARRAY colorTargetFormats{};
		uint32_t numTargets = 0;
		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (!targetSet.GetColorTarget(i))
			{
				break;
			}

			dx12::Texture::PlatformParams const* targetTexPlatParams =
				targetSet.GetColorTarget(i)->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			colorTargetFormats.RTFormats[i] = targetTexPlatParams->m_format;
			numTargets++;
		}
		SEAssert("No color targets found", numTargets > 0);
		colorTargetFormats.NumRenderTargets = numTargets;

		return colorTargetFormats;
	}
}