// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CastUtils.h"

#include <d3dx12.h>

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
				break;
			}

			dx12::TextureTarget::PlatformParams* targetPlatParams =
				colorTarget.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
			SEAssert(!targetPlatParams->m_isCreated, "Target has already been created");
			targetPlatParams->m_isCreated = true;
		}

		CreateViewportAndScissorRect(targetSet);
	}


	void TextureTargetSet::CreateDepthStencilTarget(re::TextureTargetSet const& targetSet)
	{
		if (!targetSet.HasDepthTarget())
		{
			return;
		}

		SEAssert(targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>()->m_isCommitted,
			"Target set has not been committed");

		re::TextureTarget const* depthTarget = &targetSet.GetDepthStencilTarget();

		dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
			depthTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
		SEAssert(!depthTargetPlatParams->m_isCreated, "Target has already been created");
		depthTargetPlatParams->m_isCreated = true;

		core::InvPtr<re::Texture> const& depthTex = depthTarget->GetTexture();
		re::Texture::TextureParams const& depthTexParams = depthTex->GetTextureParams();
		SEAssert(depthTexParams.m_usage & re::Texture::Usage::DepthTarget,
			"Target does not have the depth target usage type");

		// If we don't have any color targets, we must configure the viewport and scissor rect here instead
		if (!targetSet.HasColorTarget())
		{
			CreateViewportAndScissorRect(targetSet);
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