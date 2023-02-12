// © 2022 Adam Badke. All rights reserved.
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	CommandQueue_DX12::CommandQueue_DX12()
		: m_commandQueue(nullptr)
		, m_deviceCache(nullptr)
		, m_fenceValue(0)
	{
	}


	void CommandQueue_DX12::Create(ComPtr<ID3D12Device2> displayDevice, D3D12_COMMAND_LIST_TYPE type)
	{
		m_type = type;
		m_deviceCache = displayDevice; // Store a local copy, for convenience

		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type = m_type; // Direct, compute, copy, etc
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask = deviceNodeMask;

		HRESULT hr = m_deviceCache->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		CheckHResult(hr, "Failed to create command queue");

		m_fence.Create(m_deviceCache);
	}


	void CommandQueue_DX12::Destroy()
	{
		m_fence.Destroy();
	}


	void CommandQueue_DX12::Execute(uint32_t numCmdLists, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdLists[])
	{
		// Extract our raw pointers so we can execute them in a single call
		std::vector<ID3D12CommandList*> commandListPtrs;
		commandListPtrs.reserve(numCmdLists);

		std::vector<ID3D12CommandAllocator*> storedCommandAllocators;
		storedCommandAllocators.reserve(numCmdLists);

		for (uint32_t i = 0; i < numCmdLists; i++)
		{
			cmdLists[i]->Close(); // Close the command list(s) before we execute them

			commandListPtrs.emplace_back(cmdLists[i].Get());

			// Retrive the command allocator:
			ID3D12CommandAllocator* storedCmdAllocator;
			uint32_t cmdAllocatorSize = sizeof(storedCmdAllocator);
			HRESULT hr = 
				cmdLists[i]->GetPrivateData(__uuidof(ID3D12CommandAllocator), &cmdAllocatorSize, &storedCmdAllocator);
			CheckHResult(hr, "Failed to retrieve stored command allocator from command list");

			storedCommandAllocators.emplace_back(storedCmdAllocator);

			// Return our command list to the pool:
			m_commandListPool.emplace(cmdLists[i]);
		}

		// Execute the command lists:
		m_commandQueue->ExecuteCommandLists(numCmdLists, &commandListPtrs[0]);
		uint64_t fenceValue = Signal();

		// Return our command allocators to the pool:
		for (ID3D12CommandAllocator* cmdAllocator : storedCommandAllocators)
		{
			// WTF?? WE SHOULDN'T BE STORING THE RAW POINTER AS A COMPTR HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			m_commandAllocatorPool.emplace(CommandAllocatorInstance{ cmdAllocator, fenceValue });
		}
	}


	uint64_t CommandQueue_DX12::Signal()
	{
		return m_fence.Signal(m_commandQueue, m_fenceValue);
	}


	void CommandQueue_DX12::WaitForGPU(uint64_t fenceValue)
	{
		m_fence.WaitForGPU(fenceValue);
	}


	void CommandQueue_DX12::Flush()
	{
		const uint64_t fenceValueForSignal = m_fence.Signal(m_commandQueue, m_fenceValue);
		m_fence.WaitForGPU(fenceValueForSignal);
	}


	ComPtr<ID3D12GraphicsCommandList> CommandQueue_DX12::GetCreateCommandList()
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator = GetCreateCommandAllocator();

		ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;

		if (!m_commandListPool.empty())
		{
			commandList = m_commandListPool.front();
			m_commandListPool.pop();
		}
		else
		{
			// Create the command list:
			constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

			HRESULT hr = m_deviceCache->CreateCommandList(
				deviceNodeMask,
				m_type, // Direct draw/compute/copy/etc
				commandAllocator.Get(), // The command allocator the command lists will be created on
				nullptr,  // Optional: Command list initial pipeline state
				IID_PPV_ARGS(&commandList)); // Command list interface REFIID/GUID, & destination for the populated command list
			// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

			CheckHResult(hr, "Failed to create command list");

			// Note: Command lists are created in the recording state by default. The render loop resets the command 
			// list, which requires the command list to be closed. So, we pre-close new command lists so they're ready
			// to be reset before recording
			hr = commandList->Close();
			CheckHResult(hr, "Failed to close command list");
		}

		ID3D12PipelineState* pso = nullptr; // Note: pso is optional; Sets a dummy PSO if nullptr. TODO: Accept a PSO?
		commandList->Reset(commandAllocator.Get(), pso);

		// Store a pointer to the command allocator in the command list, so we can retrieve it when the command list
		// is executed
		HRESULT hr = commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get());
		CheckHResult(hr, "Failed to set private data interface");

		return commandList;
	}


	ComPtr<ID3D12CommandAllocator> CommandQueue_DX12::GetCreateCommandAllocator()
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		if (!m_commandAllocatorPool.empty() &&
			m_fence.IsFenceComplete(m_commandAllocatorPool.front().m_fenceValue))
		{
			commandAllocator = m_commandAllocatorPool.front().m_commandAllocator;
			m_commandAllocatorPool.pop();

			HRESULT hr = commandAllocator->Reset();
			CheckHResult(hr, "Failed to reset command allocator");
		}
		else
		{
			HRESULT hr = m_deviceCache->CreateCommandAllocator(
				m_type, // Copy, compute, direct draw, etc
				IID_PPV_ARGS(&commandAllocator)); // REFIID/GUID (Globally-Unique IDentifier) for the command allocator
			// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

			CheckHResult(hr, "Failed to create command allocator");
		}

		HRESULT hr = commandAllocator->Reset();
		CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}
}