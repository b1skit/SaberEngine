// � 2022 Adam Badke. All rights reserved.
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
			// RTV: Created if the texture has Texture::Usage ColorTarget or SwapchainColorProxy:
			std::vector<dx12::DescriptorAllocation> m_rtvDsvDescriptors;
		};
	};


	class TextureTargetSet
	{
	public:
		struct PlatformParams final : public re::TextureTargetSet::PlatformParams
		{
			// Target params:
			D3D12_VIEWPORT m_viewport;
			D3D12_RECT m_scissorRect;
		};


	public:

		static void CreateColorTargets(re::TextureTargetSet const& targetSet);
		static void CreateDepthStencilTarget(re::TextureTargetSet const& targetSet);

		static D3D12_RT_FORMAT_ARRAY GetColorTargetFormats(re::TextureTargetSet const& targetSet);
	};
}