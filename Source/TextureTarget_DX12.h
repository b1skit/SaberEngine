// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"


namespace dx12
{
class TextureTarget
{
public:
	struct PlatformParams final : public re::TextureTarget::PlatformParams
	{
		dx12::DescriptorAllocation m_rtvDsvDescriptor;
	};

};


class TextureTargetSet
{
public:
	struct PlatformParams final : public re::TextureTargetSet::PlatformParams
	{
		// Target formats (cached during CreateColorTargets/CreateDepthStencilTarget):
		D3D12_RT_FORMAT_ARRAY m_renderTargetFormats;
		DXGI_FORMAT m_depthTargetFormat;

		// Other target params:
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissorRect;
	};


public:

	static void CreateColorTargets(re::TextureTargetSet& targetSet);
	static void CreateDepthStencilTarget(re::TextureTargetSet& targetSet);
};
}