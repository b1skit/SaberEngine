// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "RenderManager_DX12.h"


namespace dx12
{
	class Fence
	{
	public:
		Fence();
		~Fence() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice);
		void Destroy();

		uint64_t Signal(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint64_t& fenceValue);
		void WaitForGPU(uint64_t fenceValue); // Blocks the CPU
		void Flush(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint64_t& fenceValue);


	private:
		Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
		HANDLE m_fenceEvent; // OS event object: Receives notifications when a fence reaches a specific value

	private:
		// Copying not allowed:
		Fence(Fence const&) = delete;
		Fence(Fence&&) = delete;
		Fence& operator=(Fence const&) = delete;
	};
}