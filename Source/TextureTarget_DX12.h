// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "TextureTarget.h"
#include "TextureTarget_Platform.h"


namespace dx12
{
class TextureTarget
{
public:
	struct PlatformParams final : public virtual re::TextureTarget::PlatformParams
	{

	};

};


class TextureTargetSet
{
public:
	struct PlatformParams final : public virtual re::TextureTargetSet::PlatformParams
	{
		// Color targets:
		D3D12_RT_FORMAT_ARRAY m_renderTargetFormats;
		DXGI_FORMAT m_depthTargetFormat;

		// Depth/stencil target: Depth buffer + descriptor heap for depth buffer		
		Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBufferResource = nullptr;

		// Other target params:
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;
	};


public:

	static void CreateColorTargets(re::TextureTargetSet& targetSet);

	static void CreateDepthStencilTarget(re::TextureTargetSet& targetSet);

	static void SetViewport(re::TextureTargetSet const& targetSet, ID3D12GraphicsCommandList2* commandList);
	static void SetScissorRect(re::TextureTargetSet const& targetSet, ID3D12GraphicsCommandList2* commandList);
};
}