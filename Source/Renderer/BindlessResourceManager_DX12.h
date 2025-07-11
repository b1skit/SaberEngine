// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "CommandList_DX12.h"


namespace dx12
{
	class RootSignature;


	struct IBindlessResource
	{
		static void GetResourceUseState(void* dest, size_t destByteSize);
	};


	// ---


	class BindlessResourceManager
	{
	public:
		struct PlatObj final : public virtual re::BindlessResourceManager::PlatObj
		{
			void Destroy() override;

			std::vector<std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>> m_cpuDescriptorCache; // 1 vector per frame in flight
			std::vector<ID3D12Resource*> m_resourceCache;
			std::vector<D3D12_RESOURCE_STATES> m_usageStateCache;

			ID3D12Device* m_deviceCache = nullptr;

			// We use a null descriptor to simplify book keeping around unused elements in m_cpuDescriptorCache, which
			// allows us to copy the entire range in a single call rather than checking for valid ranges to copy
			D3D12_CPU_DESCRIPTOR_HANDLE m_nullDescriptor;
			
			uint32_t m_elementSize;
			uint32_t m_numActiveResources = 0;
			uint8_t m_numFramesInFlight;


		private: // Use the static getters below:
			friend class dx12::BindlessResourceManager;
			std::unique_ptr<dx12::RootSignature> m_globalRootSig;
			std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_gpuDescriptorHeaps;
		};

	public:
		static void Initialize(re::BindlessResourceManager&, uint8_t numFramesInFlight, uint64_t frameNum);
		static void SetResource(re::BindlessResourceManager&, re::IBindlessResource*, ResourceHandle);


	public: // DX12-specific functionality:
		static std::vector<dx12::CommandList::TransitionMetadata> BuildResourceTransitions(re::BindlessResourceManager const&);


		static dx12::RootSignature const* GetRootSignature(re::BindlessResourceManager const&);

		static ID3D12DescriptorHeap* GetDescriptorHeap(re::BindlessResourceManager const&, uint64_t frameNum);
	};
}