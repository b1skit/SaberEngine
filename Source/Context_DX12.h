// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>


namespace dx12
{
	class Context
	{
	public:
		struct PlatformParams final : public virtual re::Context::PlatformParams
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter4> m_dxgiAdapter4 = nullptr;
			Microsoft::WRL::ComPtr<ID3D12Device2> m_device = nullptr; // Display adapter device

			// TODO: Many of these properties should be moved to their own object
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;

			Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;
			static const uint8_t m_numBuffers = 3; // Includes front buffer. Must be >= 2 to use the flip presentation model

			Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[m_numBuffers]; // Pointers to our backbuffer resources
			uint8_t m_backBufferIdx;

			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap; // Array of descriptors/resource views
			uint32_t m_RTVDescSize; // Stride size of a single descriptor/resource view

			// Backing memory for recording command lists into. Only reusable once commands have finished GPU execution
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[m_numBuffers];

			// Currently only 1 command list is needed as we record on a single thread. TODO: Multi-thread recording
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

			Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
			HANDLE m_fenceEvent; // OS event object: Receives notifications when a fence reaches a specific value
			uint64_t m_fenceValue = 0;

			bool m_vsyncEnabled = false; // Disabled if tearing is enabled (ie. using a variable refresh display)
			bool m_tearingSupported = false; // Always allow tearing if supported. Required for variable refresh dispays (eg. G-Sync/FreeSync)
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void SetVSyncMode(re::Context const& context, bool enabled);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static bool CheckHResult(HRESULT hr, char const* msg);
		static void EnableDebugLayer();
		static bool CheckTearingSupport(); // Variable refresh rate dispays (eg. G-Sync/FreeSync) require tearing enabled
		static Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDisplayAdapter(); // Find adapter with most VRAM
		static Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
		static Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
		static Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain(
			HWND hWnd, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t numBuffers);
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