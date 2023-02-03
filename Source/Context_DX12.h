// � 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "CommandList_DX12.h"
#include "CommandQueue_DX12.h"
#include "Context.h"
#include "Device_DX12.h"
#include "RenderManager_DX12.h"


namespace dx12
{
	class Context
	{
	public:
		struct PlatformParams final : public virtual re::Context::PlatformParams
		{
			PlatformParams();

			dx12::Device m_device;
			dx12::CommandQueue m_commandQueue;

			// TODO: Move to a "DescriptorHeapManager", owned by the Context:
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap; // Array of descriptors/resource views
			uint32_t m_RTVDescSize; // Stride size of a single descriptor/resource view

			// Currently only record a single command list per frame. TODO: Multi-thread/multi-cmd list recording
			dx12::CommandList_DX12 m_commandLists[dx12::RenderManager::k_numFrames];

			dx12::Fence m_fence;
			uint64_t m_fenceValue = 0;
			uint64_t m_frameFenceValues[dx12::RenderManager::k_numFrames]; // Fence values for signalling the command queue
		};


	public:
		Context();
		~Context() = default;

		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
		static void UpdateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device2> device,
			Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain, Microsoft::WRL::ComPtr<ID3D12Resource>* buffers, 
			uint8_t numBuffers, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap);
		static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	};
}