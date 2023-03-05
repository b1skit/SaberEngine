// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "CommandList_DX12.h"
#include "Fence_DX12.h"


namespace dx12
{
	class CommandQueue
	{
	public:
		CommandQueue();
		~CommandQueue() { Destroy(); };

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, CommandList::CommandListType type);
		void Destroy();

		// Note: shared_ptrs in cmdLists will be null after this call
		uint64_t Execute(uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists);

		uint64_t Signal();
		void WaitForGPU(uint64_t fenceValue); // Blocks the CPU
		void Flush();

		void GPUWait(uint64_t fenceValue); // Blocks the GPU

		std::shared_ptr<dx12::CommandList> GetCreateCommandList();
		

		ID3D12CommandQueue* GetD3DCommandQueue() { return m_commandQueue.Get(); }


	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		D3D12_COMMAND_LIST_TYPE m_type;

		Microsoft::WRL::ComPtr<ID3D12Device2> m_deviceCache;

		Fence m_fence;
		uint64_t m_fenceValue; // Monotonically increasing global fence value

		std::queue<std::shared_ptr<dx12::CommandList>> m_commandListPool;


	private: // No copying allowed
		CommandQueue(CommandQueue const&) = delete;
		CommandQueue(CommandQueue&&) = delete;
		CommandQueue& operator=(CommandQueue const&) = delete;
	};
}