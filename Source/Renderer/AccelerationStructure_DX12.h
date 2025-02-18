// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructure.h"
#include "HeapManager_DX12.h"

#include "Core/InvPtr.h"

#include <wrl.h>
#include <d3d12.h>


namespace dx12
{
	class CommandList;


	class AccelerationStructure
	{
	public:
		struct PlatformParams final : public re::AccelerationStructure::PlatformParams
		{
			PlatformParams();

			void Destroy() override;

			// Dependencies:
			dx12::HeapManager* m_heapManager = nullptr;
			ID3D12Device5* m_device = nullptr;

			// Resources:
			std::unique_ptr<dx12::GPUResource> m_ASBuffer;
		};


	public: // Platform functionality:
		static void Create(re::AccelerationStructure&);
		static void Destroy(re::AccelerationStructure&);


	public: // DX12-specific functionality:
		static D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildAccelerationStructureDesc(re::AccelerationStructure&);
	};

}