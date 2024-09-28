// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Sampler.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class Sampler
	{
	public:
		struct PlatformParams final : public re::Sampler::PlatformParams
		{
			D3D12_STATIC_SAMPLER_DESC m_staticSamplerDesc{};
		};


	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);
	};
}