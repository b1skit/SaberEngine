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
		CommandQueue(CommandQueue&&) noexcept = default;
		CommandQueue& operator=(CommandQueue&&) noexcept = default;
		~CommandQueue() { Destroy(); };

		[[nodiscard]] void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, dx12::CommandListType type);
		void Destroy();

		bool IsCreated() const;

		// Note: shared_ptrs in cmdLists will be null after this call
		uint64_t Execute(uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists);

		dx12::Fence& GetFence();
		dx12::Fence const& GetFence() const;
		uint64_t GetNextFenceValue() const; // The next fence value that will be used to signal

		// ID3D12Fence wrappers: CPU-side fence syncronization
		uint64_t CPUSignal(); // Updates the fence value from the CPU side
		void CPUWait(uint64_t fenceValue) const; // Blocks the CPU until the fence reaches the given value

		// ID3D12CommandQueue wrappers: GPU-side fence syncronization
		uint64_t GPUSignal(); // Updates the fence to ++m_fence value from the GPU side
		void GPUSignal(uint64_t fenceValue); // Updates the fence to the given value from the GPU side

		void GPUWait(uint64_t fenceValue) const; // Blocks the GPU until the fence reaches the given value
		void GPUWait(dx12::Fence&, uint64_t fenceValue) const; // Blocks the GPU on a fence from another command queue

		void Flush();

		std::shared_ptr<dx12::CommandList> GetCreateCommandList();
		
		ID3D12CommandQueue* GetD3DCommandQueue() const;

		CommandListType GetCommandListType() const;


	private:
		std::vector<std::shared_ptr<dx12::CommandList>> PrependBarrierCommandListsAndWaits(
			uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists);

		void TransitionIncompatibleResourceStatesToCommon(
			uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists);

		uint64_t ExecuteInternal(std::vector<std::shared_ptr<dx12::CommandList>> const&, char const* markerLabel);


	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
		CommandListType m_type;
		D3D12_COMMAND_LIST_TYPE m_d3dType;

		Microsoft::WRL::ComPtr<ID3D12Device2> m_deviceCache;

		Fence m_fence;
		uint64_t m_fenceValue; // Monotonically increasing: Most recent signalled value. Note: Pre-assigned to cmd lists
		uint64_t m_typeFenceBitMask; // Upper 3 bits indicate the fence type

		std::queue<std::shared_ptr<dx12::CommandList>> m_commandListPool;

		bool m_isCreated;


	private: // No copying allowed
		CommandQueue(CommandQueue const&) = delete;
		CommandQueue& operator=(CommandQueue const&) = delete;
	};


	inline bool CommandQueue::IsCreated() const
	{
		return m_isCreated;
	}


	inline dx12::Fence& CommandQueue::GetFence()
	{
		return m_fence;
	}


	inline dx12::Fence const& CommandQueue::GetFence() const
	{
		return m_fence;
	}


	inline uint64_t CommandQueue::GetNextFenceValue() const
	{
		return m_fenceValue + 1;
	}


	inline ID3D12CommandQueue* CommandQueue::GetD3DCommandQueue() const
	{
		return m_commandQueue.Get();
	}


	inline CommandListType CommandQueue::GetCommandListType() const
	{
		return m_type;
	}
}