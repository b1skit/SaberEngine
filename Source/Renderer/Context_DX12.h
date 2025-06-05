// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "CommandQueue_DX12.h"
#include "CPUDescriptorHeapManager_DX12.h"
#include "Device_DX12.h"
#include "HeapManager_DX12.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	class Context final : public virtual re::Context
	{
	public:
		~Context() override = default;

		// Context interface:
		void CreateInternal(uint64_t currentFrame) override;
		void UpdateInternal(uint64_t currentFrame) override;
		void DestroyInternal() override;
		
		void Present() override;

		re::BindlessResourceManager* GetBindlessResourceManager() override;


	public: // DX12-specific interface:
		dx12::CommandQueue& GetCommandQueue(dx12::CommandListType type);
		dx12::CommandQueue& GetCommandQueue(uint64_t fenceValue); // Get the command queue that produced a fence value

		dx12::PipelineState const* CreateAddPipelineState(re::Shader const&, re::TextureTargetSet const*);
			
		// A null targetSet is valid (it indicates the backbuffer, compute shaders, etc)
		dx12::PipelineState const* GetPipelineStateObject(re::Shader const&, re::TextureTargetSet const*);

		bool HasRootSignature(uint64_t rootSigDescHash);
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature(uint64_t rootSigDescHash);
		void AddRootSignature(uint64_t rootSigDescHash, Microsoft::WRL::ComPtr<ID3D12RootSignature>);
		
		dx12::CPUDescriptorHeapManager& GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType);

		dx12::Device& GetDevice();

		dx12::GlobalResourceStateTracker& GetGlobalResourceStates();

		HeapManager& GetHeapManager();
		

	public:// Null descriptor library
		DescriptorAllocation const& GetNullSRVDescriptor(D3D12_SRV_DIMENSION, DXGI_FORMAT);
		DescriptorAllocation const& GetNullUAVDescriptor(D3D12_UAV_DIMENSION, DXGI_FORMAT);
		DescriptorAllocation const& GetNullCBVDescriptor();


	private:
		std::unordered_map<D3D12_SRV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullSRVLibrary;
		std::mutex m_nullSRVLibraryMutex;
		
		std::unordered_map<D3D12_UAV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullUAVLibrary;
		std::mutex m_nullUAVLibraryMutex;

		DescriptorAllocation m_nullCBV;
		std::mutex m_nullCBVMutex;


	private:
		dx12::Device m_device;

		std::array<dx12::CommandQueue, CommandListType::CommandListType_Count> m_commandQueues;

		HeapManager m_heapManager;

		dx12::GlobalResourceStateTracker m_globalResourceStates;

		std::vector<uint64_t> m_frameFenceValues; // Fence values for signalling the command queue

		// Access the PSO library via dx12::Context::GetPipelineStateObject():
		std::unordered_map<uint64_t, std::shared_ptr<dx12::PipelineState>> m_PSOLibrary;
		std::mutex m_PSOLibraryMutex;

		// Hashed D3D12_VERSIONED_ROOT_SIGNATURE_DESC -> D3D Root sig ComPtr
		std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_rootSigLibrary;
		mutable std::mutex m_rootSigLibraryMutex;

		std::vector<dx12::CPUDescriptorHeapManager> m_cpuDescriptorHeapMgrs; // HeapType_Count

		re::BindlessResourceManager m_bindlessResourceManager;

		// PIX programmatic capture models
		HMODULE m_pixGPUCaptureModule;
		HMODULE m_pixCPUCaptureModule;


	protected:
		Context(platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window*);
		friend class re::Context;
	};


	inline re::BindlessResourceManager* Context::GetBindlessResourceManager()
	{
		return &m_bindlessResourceManager;
	}

	inline dx12::CPUDescriptorHeapManager& Context::GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType heapType)
	{
		return m_cpuDescriptorHeapMgrs[heapType];
	}


	inline dx12::Device& Context::GetDevice()
	{
		return m_device;
	}


	inline dx12::GlobalResourceStateTracker& Context::GetGlobalResourceStates()
	{
		return m_globalResourceStates;
	}


	inline HeapManager& Context::GetHeapManager()
	{
		return m_heapManager;
	}
}