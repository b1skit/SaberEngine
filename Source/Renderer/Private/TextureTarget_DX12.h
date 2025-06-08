// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/DescriptorCache_DX12.h"
#include "Private/TextureTarget.h"
#include "Private/TextureTarget_Platform.h"


namespace dx12
{
	class TextureTargetSet;


	class TextureTarget
	{
	public:
		struct PlatObj final : public re::TextureTarget::PlatObj
		{
			//
		};


	public:


	};


	class TextureTargetSet
	{
	public:
		struct PlatObj final : public re::TextureTargetSet::PlatObj
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