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
		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

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

				dx12::Texture::Create(*colorTarget.GetTexture());
			}			
		}
		SEAssert("NumRenderTargets must equal the number of format slots set", formatSlot == numColorTargets);
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

		dx12::Texture::Create(*targetSet.GetDepthStencilTarget().GetTexture());		

		targetSetParams->m_depthTargetFormat = 
			dx12::Texture::GetTextureFormat(targetSet.GetDepthStencilTarget().GetTexture()->GetTextureParams());
	}


	void TextureTargetSet::SetViewport(re::TextureTargetSet const& targetSet, ID3D12GraphicsCommandList2* commandList)
	{
		dx12::TextureTargetSet::PlatformParams* targetSetParams = 
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

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
		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

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