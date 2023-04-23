// © 2022 Adam Badke. All rights reserved.
#include "Debug_DX12.h"
#include "Fence_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	Fence::Fence()
		: m_fence(nullptr)
		, m_fenceEvent(nullptr)
	{
	}


	void Fence::Create(ComPtr<ID3D12Device2> displayDevice, char const* eventName)
	{
		// Create our fence:
		HRESULT hr = displayDevice->CreateFence(
			0,							// Initial value: Increase monotonically from hereONLY
			D3D12_FENCE_FLAG_NONE,		// Fence flags: Shared, cross-adapter, etc
			IID_PPV_ARGS(&m_fence));	// REFIIF and destination pointer for the populated fence
		CheckHResult(hr, "Failed to create fence");

		// Create our event handle:
		m_fenceEvent = ::CreateEvent(
			nullptr,	// Pointer to event SECURITY_ATTRIBUTES. If null, the handle cannot be inherited by child processes
			false,		// Manual reset: True = Reset event to non-signalled by calling ResetEvent. False = Auto-reset
			false,		// Initial state: true = signalled, false = unsignalled
			eventName);	// Event object name: Unnamed if null

		SEAssert("Failed to create fence event", m_fenceEvent);
	}


	void Fence::Destroy()
	{
		::CloseHandle(m_fenceEvent);
		m_fence = nullptr;
		m_fenceEvent = 0;
	}


	void Fence::CPUSignal(uint64_t fenceValue) const
	{
		// Updates the fence to the specified value from the CPU side
		HRESULT hr = m_fence->Signal(fenceValue);
		CheckHResult(hr, "Failed to signal fence");
	}


	void Fence::CPUWait(uint64_t fenceValue) const
	{
		// Blocks the CPU until the fence reaches the given value
		if (!IsFenceComplete(fenceValue))
		{
			HRESULT hr = m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
			CheckHResult(hr, "Failed to set completion event");

			constexpr std::chrono::milliseconds duration = std::chrono::milliseconds::max();
			::WaitForSingleObject(m_fenceEvent, static_cast<DWORD>(duration.count()));
		}
	}


	bool Fence::IsFenceComplete(uint64_t fenceValue) const
	{
		return m_fence->GetCompletedValue() >= fenceValue;
	}
}