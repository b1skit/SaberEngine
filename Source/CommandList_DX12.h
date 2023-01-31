// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>


namespace dx12
{
	class CommandList_DX12
	{
	public:
		CommandList_DX12() = default;
		~CommandList_DX12() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> device,
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator,
			D3D12_COMMAND_LIST_TYPE type);


	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	};
}