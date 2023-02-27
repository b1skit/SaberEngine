// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Fence_DX12.h"


namespace dx12
{
	class CommandQueue_DX12
	{
	public:
		enum CommandListType
		{
			Direct,
			Bundle,
			Compute,
			Copy,
			VideoDecode,
			VideoProcess,
			VideoEncode,

			CommandListType_Count
		};

	public:
		CommandQueue_DX12();
		~CommandQueue_DX12() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, CommandListType type);
		void Destroy();

		uint64_t Execute(uint32_t numCmdLists, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> cmdLists[]);

		uint64_t Signal();
		void WaitForGPU(uint64_t fenceValue); // Blocks the CPU
		void Flush();

		// TODO: Return raw pointers instead of ComPtr
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCreateCommandList();

		ID3D12CommandQueue* GetD3DCommandQueue() { return m_commandQueue.Get(); }


	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		D3D12_COMMAND_LIST_TYPE m_type;

		Microsoft::WRL::ComPtr<ID3D12Device2> m_deviceCache;

		Fence_DX12 m_fence;
		uint64_t m_fenceValue = 0;


		// Command list pool:
		std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListPool;


		// Command allocator pool:
		struct CommandAllocatorInstance
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
			uint64_t m_fenceValue;
		};
		std::queue<CommandAllocatorInstance> m_commandAllocatorPool;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCreateCommandAllocator();


	private: // No copying allowed
		CommandQueue_DX12(CommandQueue_DX12 const&) = delete;
		CommandQueue_DX12(CommandQueue_DX12&&) = delete;
		CommandQueue_DX12& operator=(CommandQueue_DX12 const&) = delete;
	};
}