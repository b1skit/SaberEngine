// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

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
			dx12::Device m_device;

			// TODO: Move to a "CommandQueue" object, owned by the Context:
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;

			// TODO: Move to a "DescriptorHeapManager", owned by the Context:
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap; // Array of descriptors/resource views
			uint32_t m_RTVDescSize; // Stride size of a single descriptor/resource view

			// TODO: Move to a "CommandList" object:
			// Backing memory for recording command lists into. Only reusable once commands have finished GPU execution
			// Note: For now, we're using one command allocator per backbuffer (i.e. 3)
			// TODO: Make this a dx12 constant, so we can access it without a PlatformParams ?????
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[dx12::RenderManager::k_numFrames];

			// TODO: Move to a "CommandList" object:
			// Currently only 1 command list is needed as we record on a single thread. TODO: Multi-thread recording
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList = nullptr;

			// TODO: Move to a "Fence" object, managed by the "Device" object:
			Microsoft::WRL::ComPtr<ID3D12Fence> m_fence = nullptr;
			HANDLE m_fenceEvent; // OS event object: Receives notifications when a fence reaches a specific value
			uint64_t m_fenceValue = 0;
			uint64_t m_frameFenceValues[dx12::RenderManager::k_numFrames] = {}; // Tracks fence values used to signal the command queue for a particular frame
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static Microsoft::WRL::ComPtr<IDXGIAdapter4> GetBestDisplayAdapter(); // Find adapter with most VRAM
		static Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
		static Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
		static void UpdateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device2> device,
			Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain, Microsoft::WRL::ComPtr<ID3D12Resource>* buffers, 
			uint8_t numBuffers, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap);
		static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
		static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator, D3D12_COMMAND_LIST_TYPE type);
		static Microsoft::WRL::ComPtr<ID3D12Fence> CreateFence(Microsoft::WRL::ComPtr<ID3D12Device2> device);
		static HANDLE CreateEventHandle();
		static void Flush(
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent);
		static uint64_t Signal(
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t& fenceValue);
		static void WaitForFenceValue(Microsoft::WRL::ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent);
	};
}