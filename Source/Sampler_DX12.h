// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Sampler.h"


namespace dx12
{
	class Sampler
	{
	public:
		struct PlatformParams final : public virtual re::Sampler::PlatformParams
		{
			PlatformParams(re::Sampler::SamplerParams const& samplerParams);
			~PlatformParams() = default;
		};


	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);
	};
}