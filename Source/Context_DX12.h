// � 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "CPUDescriptorHeapManager_DX12.h"
#include "CommandQueue_DX12.h"
#include "Context.h"
#include "Device_DX12.h"
#include "PipelineState_DX12.h"
#include "RenderManager_DX12.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	class Context final : public virtual re::Context
	{
	public:	
		~Context() override = default;

		void Create() override;

		// Platform implementations:
		static void Destroy(re::Context& context);

		// Context interface:
		void Present() override;


		// DX12-specific interface:
		dx12::CommandQueue& GetCommandQueue(dx12::CommandListType type);
		dx12::CommandQueue& GetCommandQueue(uint64_t fenceValue); // Get the command queue that produced a fence value

		std::shared_ptr<dx12::PipelineState> CreateAddPipelineState(
			re::Shader const&, re::PipelineState const&, re::TextureTargetSet const&);
			
		std::shared_ptr<dx12::PipelineState> GetPipelineStateObject(
			re::Shader const& shader,
			re::PipelineState const& rePipelineState,
			re::TextureTargetSet const* targetSet); // A null targetSet is valid (it indicates the backbuffer)

		bool HasRootSignature(uint64_t rootSigDescHash);
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature(uint64_t rootSigDescHash);
		void AddRootSignature(uint64_t rootSigDescHash, Microsoft::WRL::ComPtr<ID3D12RootSignature>);
		
		dx12::CPUDescriptorHeapManager& GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType);

		dx12::Device& GetDevice();

		ID3D12DescriptorHeap* GetImGuiGPUVisibleDescriptorHeap() const;

		dx12::GlobalResourceStateTracker& GetGlobalResourceStates();
		

	public:// Null descriptor library
		DescriptorAllocation const& GetNullSRVDescriptor(D3D12_SRV_DIMENSION, DXGI_FORMAT);
		DescriptorAllocation const& GetNullUAVDescriptor(D3D12_UAV_DIMENSION, DXGI_FORMAT);


	private:
		const D3D12_CONSTANT_BUFFER_VIEW_DESC m_nullCBV = { .BufferLocation = 0, .SizeInBytes = 32 }; // Arbitrary

		std::unordered_map<D3D12_SRV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullSRVLibrary;
		std::mutex s_nullSRVLibraryMutex;
		
		std::unordered_map<D3D12_UAV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullUAVLibrary;
		std::mutex s_nullUAVLibraryMutex;


	private:
		dx12::Device m_device;

		std::array<dx12::CommandQueue, CommandListType::CommandListType_Count> m_commandQueues;

		dx12::GlobalResourceStateTracker m_globalResourceStates;

		uint64_t m_frameFenceValues[dx12::RenderManager::GetNumFrames()]; // Fence values for signalling the command queue

		// Access the PSO library via dx12::Context::GetPipelineStateObject():
		std::unordered_map<uint64_t, std::shared_ptr<dx12::PipelineState>> m_PSOLibrary;

		// Hashed D3D12_VERSIONED_ROOT_SIGNATURE_DESC -> D3D Root sig ComPtr
		std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>> m_rootSigLibrary;
		mutable std::mutex m_rootSigLibraryMutex;


		std::vector<dx12::CPUDescriptorHeapManager> m_cpuDescriptorHeapMgrs; // HeapType_Count

		// Imgui descriptor heap: A single, CPU and GPU-visible SRV descriptor for the internal font texture
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiGPUVisibleSRVDescriptorHeap;


	protected:
		Context() = default;
		friend class re::Context;
	};


	inline dx12::CPUDescriptorHeapManager& Context::GetCPUDescriptorHeapMgr(CPUDescriptorHeapManager::HeapType heapType)
	{
		return m_cpuDescriptorHeapMgrs[heapType];
	}


	inline dx12::Device& Context::GetDevice()
	{
		return m_device;
	}


	inline ID3D12DescriptorHeap* Context::GetImGuiGPUVisibleDescriptorHeap() const
	{
		return m_imGuiGPUVisibleSRVDescriptorHeap.Get();
	}


	inline dx12::GlobalResourceStateTracker& Context::GetGlobalResourceStates()
	{
		return m_globalResourceStates;
	}
}