// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void TextureTargetSet::CreateColorTargets(re::TextureTargetSet& targetSet)
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		SEAssert("Color target is already created", !targetSetParams->m_colorIsCreated);
		targetSetParams->m_colorIsCreated = true;

		const uint8_t numColorTargets = targetSet.GetNumColorTargets();
		SEAssert("", numColorTargets > 0 && numColorTargets <= 8);

		targetSetParams->m_renderTargetFormats = {}; // Contains an array of 8 DXGI_FORMAT enums
		targetSetParams->m_renderTargetFormats.NumRenderTargets = numColorTargets;

		// Note: We pack our structure with contiguous DXGI_FORMAT's, regardless of their packing
		// in the re::TextureTargetSet slots
		uint8_t formatSlot = 0;
		for (re::TextureTarget const& colorTarget : targetSet.GetColorTargets())
		{
			if (colorTarget.HasTexture())
			{
				re::Texture::TextureParams const& texParams = colorTarget.GetTexture()->GetTextureParams();
				SEAssert("Texture has the wrong usage set", 
					texParams.m_usage == re::Texture::Usage::ColorTarget);

				targetSetParams->m_renderTargetFormats.RTFormats[formatSlot++] = 
					dx12::Texture::GetTextureFormat(texParams);

				dx12::Texture::PlatformParams* texPlatParams =
					colorTarget.GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();
				
				SEAssert("Texture is already created. This might not necessarily be a problem, but asserting for now "
					"since it's currently not expected", texPlatParams->m_isCreated == false);
				texPlatParams->m_isCreated = true;

				dx12::TextureTarget::PlatformParams* targetPlatParams = 
					colorTarget.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

				// Create the descriptor and RTV:
				targetPlatParams->m_rtvDsvDescriptor = std::move(
					ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::RTV].Allocate(1));

				SEAssert("Texture resource has not been created yet. This isn't a bug, it's just not implemented. This "
					"assert is to save some head scratching", texPlatParams->m_textureResource);

				device->CreateRenderTargetView(
					texPlatParams->m_textureResource.Get(), // Pointer to the resource containing the render target texture
					nullptr,  // Pointer to a render target view descriptor. nullptr = default
					targetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor()); // Descriptor destination
			}			
		}
		SEAssert("NumRenderTargets/numColorTargets must equal the number of format slots set", 
			formatSlot == numColorTargets);
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet& targetSet)
	{
		SEAssert("Cannot create depth stencil if target set has no depth target", targetSet.HasDepthTarget());

		dx12::TextureTargetSet::PlatformParams* targetSetParams = 
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Note: We handle this differently in OpenGL; Putting this here to help with debugging for now
		SEAssert("Depth target is already created", !targetSetParams->m_depthIsCreated);
		targetSetParams->m_depthIsCreated = true;

		SEAssert("Target has the wrong usage type", 
			targetSet.GetDepthStencilTarget().GetTexture()->GetTextureParams().m_usage == re::Texture::Usage::DepthTarget);

		std::shared_ptr<re::Texture> depthTargetTex = targetSet.GetDepthStencilTarget().GetTexture();

		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		re::Texture::TextureParams const& texParams = depthTargetTex->GetTextureParams();

		// Cache our DXGI_FORMAT:
		targetSetParams->m_depthTargetFormat = dx12::Texture::GetTextureFormat(texParams);

		// Clear values:
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = targetSetParams->m_depthTargetFormat;
		optimizedClearValue.DepthStencil = { 1.0f, 0 }; // Float depth, uint8_t stencil

		// Depth resource description:
		const int width = en::Config::Get()->GetValue<int>("windowXRes");
		const int height = en::Config::Get()->GetValue<int>("windowYRes");
		SEAssert("Invalid dimensions", width >= 1 && height >= 1);

		CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			targetSetParams->m_depthTargetFormat,
			width,
			height,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Depth committed heap resources:
		dx12::Texture::PlatformParams* texPlatParams = 
			depthTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		HRESULT hr = device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&texPlatParams->m_textureResource)
		);
		texPlatParams->m_textureResource->SetName(depthTargetTex->GetWName().c_str());

		dx12::TextureTarget::PlatformParams* targetPlatParams =
			targetSet.GetDepthStencilTarget().GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		// Create the depth-stencil descriptor and view:
		targetPlatParams->m_rtvDsvDescriptor = std::move(
			ctxPlatParams->m_cpuDescriptorHeapMgrs[dx12::Context::CPUDescriptorHeapType::DSV].Allocate(1));
		SEAssert("DSV descriptor is not valid", targetPlatParams->m_rtvDsvDescriptor.IsValid());

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = targetSetParams->m_depthTargetFormat;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(
			texPlatParams->m_textureResource.Get(),
			&dsv,
			targetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor());
	}
}