// © 2022 Adam Badke. All rights reserved.
#include "Debug_DX12.h"
#include "Fence_DX12.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	Fence_DX12::Fence_DX12()
		: m_fence(nullptr)
		, m_fenceEvent(nullptr)
	{
	}


	void Fence_DX12::Create(ComPtr<ID3D12Device2> displayDevice)
	{
		// Create our fence:
		HRESULT hr = displayDevice->CreateFence(
			0, // Initial value: It's recommended that fences always start at 0, and increase monotonically ONLY
			D3D12_FENCE_FLAG_NONE, // Fence flags: Shared, cross-adapter, etc
			IID_PPV_ARGS(&m_fence)); // REFIIF and destination pointer for the populated fence
		CheckHResult(hr, "Failed to create fence");

		// Create our event handle:
		m_fenceEvent = ::CreateEvent(
			NULL, // Pointer to event SECURITY_ATTRIBUTES. If null, the handle cannot be inherited by child processes
			FALSE, // Manual reset? If true, event must be reset to non-signalled by calling ResetEvent. Auto-resets if false
			FALSE, // Initial state: true/false = signalled/unsignalled
			NULL); // Event object name: Unnamed if null

		SEAssert("Failed to create fence event", m_fenceEvent);
	}


	void Fence_DX12::Destroy()
	{
		::CloseHandle(m_fenceEvent);
	}


	uint64_t Fence_DX12::Signal(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint64_t& fenceValue)
	{
		const uint64_t fenceValueForSignal = ++fenceValue; // Note: First fenceValueForSignal == 1

		HRESULT hr = commandQueue->Signal(
			m_fence.Get(), // Fence object ptr
			fenceValueForSignal); // Value to signal the fence with

		CheckHResult(hr, "Failed to signal fence");

		return fenceValueForSignal;
	}


	void Fence_DX12::WaitForGPU(uint64_t fenceValue)
	{
		if (m_fence->GetCompletedValue() < fenceValue)
		{
			HRESULT hr = m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
			CheckHResult(hr, "Failed to set completion event");

			constexpr std::chrono::milliseconds duration = std::chrono::milliseconds::max();
			::WaitForSingleObject(m_fenceEvent, static_cast<DWORD>(duration.count()));
		}
	}


	void Fence_DX12::Flush(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint64_t& fenceValue)
	{
		const uint64_t fenceValueForSignal = Signal(commandQueue, fenceValue);
		WaitForGPU(fenceValueForSignal);
	}
}