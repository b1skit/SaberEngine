// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

struct CD3DX12_RESOURCE_BARRIER;
struct CD3DX12_CPU_DESCRIPTOR_HANDLE;


namespace dx12
{
	class CommandList_DX12
	{
	public:
		CommandList_DX12();
		~CommandList_DX12() = default;

		void Create(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);

		void AddResourceBarrier(uint8_t numBarriers, CD3DX12_RESOURCE_BARRIER* const barriers);
		void Close();
		void Reset(ID3D12PipelineState* pso);
		void ClearRTV(CD3DX12_CPU_DESCRIPTOR_HANDLE& rtv, glm::vec4 const& clearColor, uint8_t numRects, D3D12_RECT const* rects);

		inline Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetD3DCommandList() const { return m_commandList; }

	private:
		void CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);


	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
		
		// Backing memory for command list recording. Only reusable once commands have finished GPU execution
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;		
	};
}