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
		dx12::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		// Note: We handle this differently in OpenGL; Putting this here to help with debugging for now
		SEAssert("Color target is already created", !targetSetParams->m_colorIsCreated);
		targetSetParams->m_colorIsCreated = true;

		const uint8_t numColorTargets = targetSet.GetNumColorTargets();
		SEAssert("", numColorTargets > 0 && numColorTargets <= 8);

		targetSetParams->m_renderTargetFormats = {}; // Contains an array of 8 DXGI_FORMAT enums
		targetSetParams->m_renderTargetFormats.NumRenderTargets = numColorTargets;

		// Note: We pack our structure with contiguous render target format descriptions, regardless of their packing
		// in the re::TextureTargetSet slots
		uint8_t formatSlot = 0;
		for (re::TextureTarget const& colorTarget : targetSet.GetColorTargets())
		{
			if (colorTarget.HasTexture())
			{
				targetSetParams->m_renderTargetFormats.RTFormats[formatSlot++] = 
					dx12::Texture::GetTextureFormat(colorTarget.GetTexture()->GetTextureParams());
			}			
		}
		SEAssert("NumRenderTargets must equal the number of format slots set", formatSlot == numColorTargets);
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet& targetSet)
	{
		// NOTE: We assume the depth buffer is not in use. If this ever changes (eg. we're recreating the depth buffer,
		// and it may still be in flight, we should ensure we flush all commands that might reference it first

		dx12::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		// Note: We handle this differently in OpenGL; Putting this here to help with debugging for now
		SEAssert("Depth target is already created", !targetSetParams->m_depthIsCreated);
		targetSetParams->m_depthIsCreated = true;

		targetSetParams->m_depthTargetFormat = 
			dx12::Texture::GetTextureFormat(targetSet.GetDepthStencilTarget().GetTexture()->GetTextureParams());

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(re::RenderManager::Get()->GetContext().GetPlatformParams());

		// Create the descriptor heap for the depth-stencil view:
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		HRESULT hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&targetSetParams->m_DSVHeap));
		CheckHResult(hr, "Failed to create descriptor heap");

		const int width = en::Config::Get()->GetValue<int>("windowXRes");
		const int height = en::Config::Get()->GetValue<int>("windowYRes");
		SEAssert("Invalid dimensions", width >= 1 && height >= 1);

		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = targetSetParams->m_depthTargetFormat;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		CD3DX12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		CD3DX12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			targetSetParams->m_depthTargetFormat,
			width,
			height,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		hr = device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&targetSetParams->m_depthBufferResource)
		);

		// Update the depth-stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = targetSetParams->m_depthTargetFormat;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(
			targetSetParams->m_depthBufferResource.Get(),
			&dsv,
			targetSetParams->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
	}


	void TextureTargetSet::SetViewport(re::TextureTargetSet const& targetSet, ID3D12GraphicsCommandList2* commandList)
	{
		dx12::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		re::Viewport const& viewport = targetSet.Viewport();

		// TODO: We should only update this if it has changed!
		// TODO: OpenGL expects ints, DX12 expects floats. We should support both via the Viewport interface (eg. Union)!
		targetSetParams->m_viewport = CD3DX12_VIEWPORT(
			static_cast<float>(viewport.xMin()),
			static_cast<float>(viewport.yMin()),
			static_cast<float>(viewport.Width()),
			static_cast<float>(viewport.Height()));

		commandList->RSSetViewports(1, &targetSetParams->m_viewport);

		// TODO: It is possible to have more than 1 viewport (eg. Geometry shaders), we should handle this (i.e. a 
		// viewport per target?)
	}


	void TextureTargetSet::SetScissorRect(re::TextureTargetSet const& targetSet, ID3D12GraphicsCommandList2* commandList)
	{
		dx12::TextureTargetSet::PlatformParams* const targetSetParams =
			dynamic_cast<dx12::TextureTargetSet::PlatformParams*>(targetSet.GetPlatformParams());

		re::ScissorRect const& scissorRect = targetSet.ScissorRect();

		// TODO: We should only update this if it has changed!
		targetSetParams->m_scissorRect = CD3DX12_RECT(
			scissorRect.Left(),
			scissorRect.Top(),
			scissorRect.Right(),
			scissorRect.Bottom());

		commandList->RSSetScissorRects(1, &targetSetParams->m_scissorRect);
	}
}