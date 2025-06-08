// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Private/AccelerationStructure.h"
#include "Private/CPUDescriptorHeapManager_DX12.h"
#include "Private/HeapManager_DX12.h"


namespace dx12
{
	class AccelerationStructure
	{
	public:
		struct PlatObj final : public re::AccelerationStructure::PlatObj
		{
			PlatObj();

			void Destroy() override;

			// Dependencies:
			dx12::HeapManager* m_heapManager = nullptr;
			ID3D12Device5* m_device = nullptr;

			// Resources:
			std::unique_ptr<dx12::GPUResource> m_ASBuffer;

			dx12::DescriptorAllocation m_tlasSRV; // Invalid/unused for BLAS's
		};


	public: // Platform functionality:
		static void Create(re::AccelerationStructure&);
		static void Destroy(re::AccelerationStructure&);


	public: // DX12-specific functionality:
		static void BuildAccelerationStructure(re::AccelerationStructure&, bool doUpdate, ID3D12GraphicsCommandList4*);
	};

}