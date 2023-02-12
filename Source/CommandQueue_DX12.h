// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence_DX12.h"


namespace dx12
{
	class CommandList_DX12;


	class CommandQueue_DX12
	{
	public:
		CommandQueue_DX12();
		~CommandQueue_DX12() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, D3D12_COMMAND_LIST_TYPE type);
		void Destroy();

		void Execute(uint32_t numCmdLists, ID3D12CommandList* const* cmdLists);

		uint64_t Signal();
		void WaitForGPU(uint64_t fenceValue); // Blocks the CPU
		void Flush();

		//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList();

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3DCommandQueue() { return m_commandQueue; }


	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

		Fence_DX12 m_fence;
		uint64_t m_fenceValue = 0;

		//std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>> m_commandListPool;
		//std::queue<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commandAllocatorPool;


	private: // No copying allowed
		CommandQueue_DX12(CommandQueue_DX12 const&) = delete;
		CommandQueue_DX12(CommandQueue_DX12&&) = delete;
		CommandQueue_DX12& operator=(CommandQueue_DX12 const&) = delete;
	};
}