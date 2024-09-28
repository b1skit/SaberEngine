// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "DescriptorCache_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_Platform.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class TextureTargetSet;


	class TextureTarget
	{
	public:
		struct PlatformParams final : public re::TextureTarget::PlatformParams
		{
			//
		};


	public:


	};


	class TextureTargetSet
	{
	public:
		struct PlatformParams final : public re::TextureTargetSet::PlatformParams
		{
			D3D12_VIEWPORT m_viewport{};
			D3D12_RECT m_scissorRect{};
		};


	public:
		static void CreateColorTargets(re::TextureTargetSet const& targetSet);
		static void CreateDepthStencilTarget(re::TextureTargetSet const& targetSet);

		static D3D12_RT_FORMAT_ARRAY GetColorTargetFormats(re::TextureTargetSet const& targetSet);
	};
}