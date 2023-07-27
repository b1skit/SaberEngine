// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "CommandList_DX12.h"
#include "RenderManager_DX12.h"


namespace dx12
{
	class Fence
	{
	public:
		static uint64_t GetCommandListTypeFenceMaskBits(dx12::CommandListType commandListType);
		static dx12::CommandListType GetCommandListTypeFromFenceValue(uint64_t fenceVal);
		static uint64_t GetRawFenceValue(uint64_t);


	public:
		Fence();
		Fence(Fence&&) = default;
		Fence& operator=(Fence&&) = default;
		~Fence() { Destroy(); };

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, char const* eventName);
		void Destroy();

		void CPUSignal(uint64_t fenceValue) const; // Updates the fence to the given value from the CPU side
		void CPUWait(uint64_t fenceValue) const; // Blocks the CPU until the fence reaches the given value
		
		bool IsFenceComplete(uint64_t fenceValue) const;

		ID3D12Fence* GetD3DFence() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		HANDLE m_fenceEvent; // OS event object: Receives notifications when a fence reaches a specific value


	private: // No copying allowed:
		Fence(Fence const&) = delete;
		Fence& operator=(Fence const&) = delete;
	};


	inline ID3D12Fence* Fence::GetD3DFence() const
	{
		return m_fence.Get();
	}
}