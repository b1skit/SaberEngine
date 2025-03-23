// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "CommandList_DX12.h"
#include "RootSignature_DX12.h"

#include <d3d12.h>
#include <wrl.h>


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
			uint32_t m_numActiveResources = 0;
		};


	public:
		static void Initialize(re::IBindlessResourceSet&);
		static void SetResource(re::IBindlessResourceSet&, re::IBindlessResource*, ResourceHandle);
	};


	// ---


	class BindlessResourceManager
	{
	public:
		struct PlatformParams : public re::BindlessResourceManager::PlatformParams
		{
			void Destroy() override;

			std::unique_ptr<dx12::RootSignature> m_rootSignature;

			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_gpuCbvSrvUavDescriptorHeap;
			size_t m_elementSize;

			ID3D12Device* m_deviceCache = nullptr;

			static constexpr D3D12_DESCRIPTOR_HEAP_TYPE k_brmHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		};


	public:
		static void Create(re::BindlessResourceManager&, uint32_t totalDescriptors);


	public: // DX12-specific functionality:
		static ID3D12RootSignature* GetRootSignature(re::BindlessResourceManager const&);
		static ID3D12DescriptorHeap* GetDescriptorHeap(re::BindlessResourceManager const&);
		static std::vector<dx12::CommandList::TransitionMetadata> BuildResourceTransitions(re::BindlessResourceManager const&);
	};
}