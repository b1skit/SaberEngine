// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Sampler.h"


namespace dx12
{
	class Sampler
	{
	public:
		struct PlatObj final : public re::Sampler::PlatObj
		{
			D3D12_STATIC_SAMPLER_DESC m_staticSamplerDesc{};
		};


	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);
	};
}