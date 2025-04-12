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
	void GPUTimer::PlatObj::Destroy()
	{
		m_directComputeQueryHeap = nullptr;
		m_directComputeQueryBuffer = nullptr;

		m_copyQueryHeap = nullptr;
		m_copyQueryBuffer = nullptr;
	}


	void GPUTimer::Create(re::GPUTimer const& timer)
	{
		dx12::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<dx12::GPUTimer::PlatObj*>();

		dx12::Context* dx12Context = re::Context::Get()->GetAs<dx12::Context*>();

		Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = dx12Context->GetDevice().GetD3DDevice();

		const uint8_t totalQueriesPerTimer = platObj->m_numFramesInFlight * 2; // x2 for start/end timestamps
		const uint32_t totalQuerySlots = totalQueriesPerTimer * re::GPUTimer::k_maxGPUTimersPerFrame;
		const uint64_t totalQueryBytes = static_cast<uint64_t>(totalQuerySlots) * k_queryElementSize;

		platObj->m_totalQueryBytesPerFrame = re::GPUTimer::k_maxGPUTimersPerFrame * k_queryElementSize * 2;

		// Get the GPU timestamp frequency:
		uint64_t gpuFrequency = 0;
		HRESULT hr = dx12Context->GetCommandQueue(
			dx12::CommandListType::Direct).GetD3DCommandQueue()->GetTimestampFrequency(&gpuFrequency);
		CheckHResult(hr, "Failed to get timestamp frequency");

		platObj->m_invGPUFrequency = 1000.0 / static_cast<double>(gpuFrequency); // Ticks/second (Hz) -> ticks/ms

		auto CreateQueryResources = [&d3dDevice](
			uint32_t totalQuerySlots,
			uint64_t totalQueryBytes,
			D3D12_QUERY_HEAP_TYPE queryHeapType,
			Microsoft::WRL::ComPtr<ID3D12QueryHeap>& queryHeapOut,
			Microsoft::WRL::ComPtr<ID3D12Resource>& queryBufferOut)
			{
				// Direct and compute command list query heap:
				const D3D12_QUERY_HEAP_DESC queryHeapdesc = {
					.Type = queryHeapType,
					.Count = totalQuerySlots,
					.NodeMask = dx12::SysInfo::GetDeviceNodeMask(),
				};

				HRESULT hr = d3dDevice->CreateQueryHeap(&queryHeapdesc, IID_PPV_ARGS(&queryHeapOut));
				CheckHResult(hr, "Failed to create query heap");

				// Direct and compute command list readback resource:
				D3D12_HEAP_PROPERTIES const& gpuTimerReadbackHeapProperties = 
					CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

				D3D12_RESOURCE_DESC const& bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalQueryBytes);

				hr = d3dDevice->CreateCommittedResource(
					&gpuTimerReadbackHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&queryBufferOut));

				CheckHResult(hr, "Failed to create query heap");
			};

		// Direct and compute command list queries:
		CreateQueryResources(
			totalQuerySlots, 
			totalQueryBytes, 
			D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 
			platObj->m_directComputeQueryHeap, 
			platObj->m_directComputeQueryBuffer);

		platObj->m_directComputeQueryHeap->SetName(L"Direct/Compute GPU Timer query heap");
		platObj->m_directComputeQueryBuffer->SetName(L"Direct/Compute GPU Timer query buffer");

		// Copy command list queries (if supported):
		D3D12_FEATURE_DATA_D3D12_OPTIONS3 const* options3 = static_cast<D3D12_FEATURE_DATA_D3D12_OPTIONS3 const*>(
			dx12::SysInfo::GetD3D12FeatureSupportData(D3D12_FEATURE_D3D12_OPTIONS3));

		platObj->m_copyQueriesSupported = options3->CopyQueueTimestampQueriesSupported;
		if (platObj->m_copyQueriesSupported)
		{
			CreateQueryResources(
				totalQuerySlots,
				totalQueryBytes,
				D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP,
				platObj->m_copyQueryHeap,
				platObj->m_copyQueryBuffer);

			platObj->m_copyQueryHeap->SetName(L"Copy GPU Timer query heap");
			platObj->m_copyQueryBuffer->SetName(L"Copy GPU Timer query buffer");
		}
	}


	void GPUTimer::BeginFrame(re::GPUTimer const&)
	{
		//
	}


	std::vector<uint64_t> GPUTimer::EndFrame(
		re::GPUTimer const& timer, re::GPUTimer::TimerType timerType)
	{
		dx12::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<dx12::GPUTimer::PlatObj*>();

		if (timerType == re::GPUTimer::TimerType::Copy && !platObj->m_copyQueriesSupported)
		{
			return {};
		}

		dx12::CommandQueue* cmdQueue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> queryBuffer;
		uint8_t timerCount = 0;
		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			cmdQueue = &re::Context::Get()->GetAs<dx12::Context*>()->GetCommandQueue(dx12::CommandListType::Direct);

			queryHeap = platObj->m_directComputeQueryHeap;
			queryBuffer = platObj->m_directComputeQueryBuffer;
			timerCount = platObj->m_currentDirectComputeTimerCount;
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			cmdQueue = &re::Context::Get()->GetAs<dx12::Context*>()->GetCommandQueue(dx12::CommandListType::Copy);

			queryHeap = platObj->m_copyQueryHeap;
			queryBuffer = platObj->m_copyQueryBuffer;
			timerCount = platObj->m_currentCopyTimerCount;
		}
		break;
		default: SEAssertF("Invalid timer type");
		}

		const uint8_t frameIdx = platObj->m_currentFrameIdx;

		// Schedule readbacks of the current frame's queries:
		if (timerCount > 0)
		{
			const uint32_t totalQueries = timerCount * 2;

			const uint32_t queryStartIdx = frameIdx * re::GPUTimer::k_maxGPUTimersPerFrame * 2;
			const uint64_t alignedDestBufferOffset = frameIdx * platObj->m_totalQueryBytesPerFrame;

			std::shared_ptr<dx12::CommandList> cmdList = cmdQueue->GetCreateCommandList();

			// Record a command to resolve the current frame's start/end queries:
			cmdList->GetD3DCommandList()->ResolveQueryData(
				queryHeap.Get(),				// Query heap to resolve
				D3D12_QUERY_TYPE_TIMESTAMP,		// Query type
				queryStartIdx,					// Start index
				totalQueries,					// No. queries
				queryBuffer.Get(),				// Destination buffer
				alignedDestBufferOffset);		// Aligned destination buffer offset

			cmdQueue->Execute(1, &cmdList);
		}

		// Readback our oldest queries:
		std::vector<uint64_t> gpuTimes(re::GPUTimer::k_maxGPUTimersPerFrame * 2, 0);

		const uint8_t oldestFrameIdx = (frameIdx + 1) % platObj->m_numFramesInFlight;

		const size_t firstReadbackByte =
			static_cast<uint64_t>(oldestFrameIdx) * platObj->m_totalQueryBytesPerFrame;
		const size_t endReadbackByte =
			firstReadbackByte + platObj->m_totalQueryBytesPerFrame; // One-past-the-end
		const D3D12_RANGE readbackRange{ .Begin = firstReadbackByte, .End = endReadbackByte };

		uint64_t* timingSrcData = nullptr;
		const HRESULT hr =
			queryBuffer->Map(0, &readbackRange, reinterpret_cast<void**>(&timingSrcData));
		CheckHResult(hr, "Failed to map GPU timer query buffer");

		memcpy(gpuTimes.data(), timingSrcData, platObj->m_totalQueryBytesPerFrame);

		queryBuffer->Unmap(0, nullptr);

		return gpuTimes;
	}


	void GPUTimer::StartTimer(
		re::GPUTimer const& timer, re::GPUTimer::TimerType timerType, uint32_t startQueryIdx, void* platformObject)
	{
		dx12::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<dx12::GPUTimer::PlatObj*>();

		if (timerType == re::GPUTimer::TimerType::Copy && !platObj->m_copyQueriesSupported)
		{
			return;
		}

		ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(platformObject);

		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ||
				cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE,
				"TimerType and command list type mismatch");

			cmdList->EndQuery(platObj->m_directComputeQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COPY,
				"TimerType and command list type mismatch");

			cmdList->EndQuery(platObj->m_copyQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, startQueryIdx);
		}
		break;
		default: SEAssertF("Invalid timer type");
		}
	}
	
	
	void GPUTimer::StopTimer(
		re::GPUTimer const& timer, re::GPUTimer::TimerType timerType, uint32_t endQueryIdx, void* platformObject)
	{
		dx12::GPUTimer::PlatObj* platObj = timer.GetPlatformObject()->As<dx12::GPUTimer::PlatObj*>();

		if (timerType == re::GPUTimer::TimerType::Copy && !platObj->m_copyQueriesSupported)
		{
			return;
		}

		ID3D12GraphicsCommandList* cmdList = static_cast<ID3D12GraphicsCommandList*>(platformObject);

		switch (timerType)
		{
		case re::GPUTimer::TimerType::DirectCompute:
		{
			SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT ||
				cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COMPUTE,
				"TimerType and command list type mismatch");

			cmdList->EndQuery(platObj->m_directComputeQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);
		}
		break;
		case re::GPUTimer::TimerType::Copy:
		{
			SEAssert(cmdList->GetType() == D3D12_COMMAND_LIST_TYPE_COPY,
				"TimerType and command list type mismatch");

			cmdList->EndQuery(platObj->m_copyQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQueryIdx);
		}
		break;
		default: SEAssertF("Invalid timer type");
		}
	}
}
