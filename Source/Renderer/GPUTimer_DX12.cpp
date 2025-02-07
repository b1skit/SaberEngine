// © 2025 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Device_DX12.h"
#include "GPUTimer_DX12.h"
#include "SysInfo_DX12.h"

#include "Core/PerfLogger.h"

#include <d3dx12.h>


namespace
{
	constexpr uint8_t k_queryElementSize = sizeof(uint64_t);
}

namespace dx12
{
	void GPUTimer::PlatformParams::Destroy()
	{
		m_gpuQueryHeap = nullptr;
		m_gpuQueryBuffer = nullptr;
	}


	void GPUTimer::Create(re::GPUTimer const& timer)
	{
		dx12::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<dx12::GPUTimer::PlatformParams*>();

		dx12::Context* dx12Context = re::Context::Get()->GetAs<dx12::Context*>();

		const uint8_t totalQueriesPerTimer = platParams->m_numFramesInFlight * 2; // x2 for start/end timestamps
		const uint32_t totalQuerySlots = totalQueriesPerTimer * re::GPUTimer::k_maxGPUTimersPerFrame;
		const uint64_t totalQueryBytes = static_cast<uint64_t>(totalQuerySlots) * k_queryElementSize;

		platParams->m_totalQueryBytesPerFrame = re::GPUTimer::k_maxGPUTimersPerFrame * k_queryElementSize * 2;

		const D3D12_QUERY_HEAP_DESC queryHeapdesc = {
			.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
			.Count = totalQuerySlots,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask(),
		};

		ID3D12Device2* d3dDevice = dx12Context->GetDevice().GetD3DDisplayDevice();
		HRESULT hr = d3dDevice->CreateQueryHeap(
			&queryHeapdesc,
			IID_PPV_ARGS(&platParams->m_gpuQueryHeap));
		CheckHResult(hr, "Failed to create query heap");

		platParams->m_gpuQueryHeap->SetName(L"GPU Timer query heap");		

		// Get the GPU timestamp frequency:
		uint64_t gpuFrequency = 0;
		hr = dx12Context->GetCommandQueue(
			dx12::CommandListType::Direct).GetD3DCommandQueue()->GetTimestampFrequency(&gpuFrequency);
		CheckHResult(hr, "Failed to get timestamp frequency");

		platParams->m_invGPUFrequency = 1000.0 / static_cast<double>(gpuFrequency); // Ticks/second (Hz) -> ticks/ms

		// Create a buffer and readback heap:
		const D3D12_HEAP_PROPERTIES gpuTimerReadbackHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		const D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalQueryBytes);

		hr = d3dDevice->CreateCommittedResource(
			&gpuTimerReadbackHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&platParams->m_gpuQueryBuffer));

		CheckHResult(hr, "Failed to create query heap");

		platParams->m_gpuQueryBuffer->SetName(L"GPU Timer query buffer");
	}


	void GPUTimer::Destroy(re::GPUTimer const&)
	{
		//
	}


	void GPUTimer::BeginFrame(re::GPUTimer const&)
	{
		//
	}


	std::vector<uint64_t> GPUTimer::EndFrame(re::GPUTimer const& timer, void* platformObject)
	{
		ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(platformObject);

		SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ||
			cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Command list does not support timestamp queries");

		dx12::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<dx12::GPUTimer::PlatformParams*>();
	
		const uint8_t frameIdx = (platParams->m_currentFrameNum % platParams->m_numFramesInFlight);

		// Schedule readbacks of the current frame's queries:
		if (platParams->m_currentFrameTimerCount > 0)
		{
			const uint32_t totalQueries = platParams->m_currentFrameTimerCount * 2;

			const uint32_t queryStartIdx = frameIdx * re::GPUTimer::k_maxGPUTimersPerFrame * 2;
			const uint64_t alignedDestBufferOffset = frameIdx * platParams->m_totalQueryBytesPerFrame;

			// Record a command to resolve the current frame's start/end queries:
			cmdList->ResolveQueryData(
				platParams->m_gpuQueryHeap.Get(),	// Query heap to resolve
				D3D12_QUERY_TYPE_TIMESTAMP,			// Query type
				queryStartIdx,						// Start index
				totalQueries,						// No. queries
				platParams->m_gpuQueryBuffer.Get(),	// Destination buffer
				alignedDestBufferOffset);			// Aligned destination buffer offset
		}

		// Readback our oldest queries:
		std::vector<uint64_t> gpuTimes(re::GPUTimer::k_maxGPUTimersPerFrame * 2, 0);

		const uint8_t oldestFrameIdx = (frameIdx + 1) % platParams->m_numFramesInFlight;

		const size_t firstReadbackByte =
			static_cast<uint64_t>(oldestFrameIdx) * platParams->m_totalQueryBytesPerFrame;
		const size_t endReadbackByte =
			firstReadbackByte + platParams->m_totalQueryBytesPerFrame; // One-past-the-end
		const D3D12_RANGE readbackRange{ .Begin = firstReadbackByte, .End = endReadbackByte };

		uint64_t* timingSrcData = nullptr;
		const HRESULT hr =
			platParams->m_gpuQueryBuffer->Map(0, &readbackRange, reinterpret_cast<void**>(&timingSrcData));
		CheckHResult(hr, "Failed to map GPU timer query buffer");

		memcpy(gpuTimes.data(), timingSrcData, platParams->m_totalQueryBytesPerFrame);

		platParams->m_gpuQueryBuffer->Unmap(0, nullptr);

		return gpuTimes;
	}


	void GPUTimer::StartTimer(re::GPUTimer const& timer, uint32_t startQueryIdx, void* platformObject)
	{
		ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(platformObject);
		
		SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ||
			cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Command list does not support timestamp queries");		

		dx12::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<dx12::GPUTimer::PlatformParams*>();
		
		cmdList->EndQuery(platParams->m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);
	}
	
	
	void GPUTimer::StopTimer(re::GPUTimer const& timer, uint32_t endQueryIdx, void* platformObject)
	{
		ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(platformObject);

		SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ||
			cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Command list does not support timestamp queries");

		dx12::GPUTimer::PlatformParams* platParams = timer.GetPlatformParams()->As<dx12::GPUTimer::PlatformParams*>();

		cmdList->EndQuery(platParams->m_gpuQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);
	}
}
