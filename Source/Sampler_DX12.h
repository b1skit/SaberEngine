// � 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "Sampler.h"


namespace dx12
{
	class Sampler
	{
	public:
		struct PlatformParams final : public re::Sampler::PlatformParams
		{
			PlatformParams(re::Sampler::SamplerParams const& samplerParams);
			
			//DescriptorAllocation m_cpuDescAllocation;

			//D3D12_STATIC_SAMPLER_DESC m_staticSamplerDesc;
		};


	public:
		static void Create(re::Sampler& sampler);
		static void Destroy(re::Sampler& sampler);
	};
}