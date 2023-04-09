// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "ParameterBlock.h"


namespace dx12
{
	class ParameterBlock
	{
	public:
		struct PlatformParams final : public re::ParameterBlock::PlatformParams
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBufferResource = nullptr;
			DescriptorAllocation m_cpuDescAllocation;
		};


	public:
		// Platform handles:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock& paramBlock);
		static void Destroy(re::ParameterBlock& paramBlock);
	};
}