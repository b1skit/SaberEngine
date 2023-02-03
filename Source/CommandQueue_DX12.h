// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>


namespace dx12
{
	class CommandQueue
	{
	public:
		CommandQueue();
		~CommandQueue() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> displayDevice, D3D12_COMMAND_LIST_TYPE type);

		void Execute(uint32_t numCmdLists, ID3D12CommandList* const* cmdLists);

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3DCommandQueue() { return m_commandQueue; }


	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

	};
}