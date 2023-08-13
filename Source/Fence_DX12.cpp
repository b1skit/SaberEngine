// © 2022 Adam Badke. All rights reserved.
#include <pix3.h>

#include "Debug_DX12.h"
#include "Fence_DX12.h"
#include "TextUtils.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	constexpr uint64_t k_reservedBits = 3; // We reserved the upper 3 bits for values [0,7]
	constexpr uint64_t k_bitShiftWidth = 64 - k_reservedBits;
	constexpr uint64_t k_commandListTypeBitmask = 7ull << k_bitShiftWidth;


	uint64_t Fence::GetCommandListTypeFenceMaskBits(dx12::CommandListType commandListType)
	{
		const uint64_t typeBits = static_cast<uint64_t>(commandListType);
		SEAssert("Unexpected command list cast results", typeBits < 7);

		return typeBits << k_bitShiftWidth;
	}


	dx12::CommandListType Fence::GetCommandListTypeFromFenceValue(uint64_t fenceVal)
	{
		const uint64_t isolatedTypeBits = fenceVal & k_commandListTypeBitmask;
		const uint64_t shiftedBits = isolatedTypeBits >> k_bitShiftWidth;
		return static_cast<dx12::CommandListType>(shiftedBits);
	}


	uint64_t Fence::GetRawFenceValue(uint64_t fenceVal)
	{
		return (fenceVal << k_reservedBits) >> k_reservedBits;
	}


	Fence::Fence()
		: m_fence(nullptr)
		, m_fenceEvent(nullptr)
		, m_mostRecentlyConfirmedFence(0)
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

		m_fence->SetName(util::ToWideString(eventName).c_str());
	}


	void Fence::Destroy()
	{
		::CloseHandle(m_fenceEvent);
		m_fence = nullptr;
		m_fenceEvent = 0;
	}


	void Fence::CPUSignal(uint64_t fenceValue)
	{
		// Updates the fence to the specified value from the CPU side
		HRESULT hr = m_fence->Signal(fenceValue);
		CheckHResult(hr, "Failed to signal fence");
		m_mostRecentlyConfirmedFence = fenceValue;
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

			// Once we get to this point, the event has been successfully signaled so we notify PIX
			PIXNotifyWakeFromFenceSignal(m_fenceEvent);
		}
	}


	bool Fence::IsFenceComplete(uint64_t fenceValue) const
	{
		if (fenceValue > m_mostRecentlyConfirmedFence)
		{
			m_mostRecentlyConfirmedFence = m_fence->GetCompletedValue();
		}
		return m_mostRecentlyConfirmedFence >= fenceValue;
	}
}