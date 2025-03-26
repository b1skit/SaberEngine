// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "CommandList_DX12.h"

#include <d3d12.h>


namespace dx12
{
	class IBindlessResourceSet
	{
	public:
		struct PlatformParams : public re::IBindlessResourceSet::PlatformParams
		{
			void Destroy() override;

			ID3D12Device* m_deviceCache = nullptr;

			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_cpuDescriptorCache;
			std::vector<ID3D12Resource*> m_resourceCache;

			D3D12_CPU_DESCRIPTOR_HANDLE m_nullDescriptor{0};
			D3D12_RESOURCE_STATES m_usageState;

			uint32_t m_numActiveResources = 0;
		};


	public:
		static void Initialize(re::IBindlessResourceSet&);
		static void SetResource(re::IBindlessResourceSet&, re::IBindlessResource*, ResourceHandle);
	};


	// ---


	class BindlessResourceManager
	{
	public: // DX12-specific functionality:
		static std::vector<dx12::CommandList::TransitionMetadata> BuildResourceTransitions(re::BindlessResourceManager const&);
	};
}