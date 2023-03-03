// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "CommandQueue_DX12.h"
#include "Context.h"
#include "Device_DX12.h"
#include "PipelineState_DX12.h"
#include "RenderManager_DX12.h"


namespace dx12
{
	class Context
	{
	public:
		static constexpr uint32_t k_numRTVDescriptors = dx12::RenderManager::k_numFrames;
		static constexpr uint32_t k_numDSVDescriptors = 1;
		// TODO: Move these to a descriptor heap manager, and choose sensible larger values

	public:
		struct PlatformParams final : public virtual re::Context::PlatformParams
		{
			PlatformParams();

			dx12::Device_DX12 m_device;

			std::array<dx12::CommandQueue_DX12, CommandQueue_DX12::CommandListType::CommandListType_Count> m_commandQueues;

			uint64_t m_frameFenceValues[dx12::RenderManager::k_numFrames]; // Fence values for signalling the command queue

			// TODO: Move to a "DescriptorHeapManager", owned by the Context
			// -> Need a helper: GetCurrentBackbufferRTVDescriptor
			// -> For now: Owned by TextureTargetSet?
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVDescHeap; // ComPtr to an array of RTV descriptors
			uint32_t m_RTVDescSize; // Stride size of a single RTV descriptor/resource view
			// NOTE: Currently, we create k_numFrames descriptors (1 for each frame) during Context::Create()

			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap = nullptr; // ComPtr to an array of DSV descriptors
			uint32_t m_DSVDescSize;

			// TODO: Precompute a library of all pipeline states needed at startup. For now, we just have a single PSO
			std::shared_ptr<dx12::PipelineState> m_pipelineState;
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static std::shared_ptr<dx12::PipelineState> CreateAddPipelineState(
			gr::PipelineState const&, re::Shader const&, D3D12_RT_FORMAT_ARRAY const& rtvFormats, const DXGI_FORMAT dsvFormat);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static dx12::CommandQueue_DX12& GetCommandQueue(CommandQueue_DX12::CommandListType type);

		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

		// TODO:
		// Add a helper wrapper to get:
		// - The current backbuffer index from the swapchain: dx12::SwapChain::GetBackBufferIdx
		// - The swapchain backbuffer resource: dx12::SwapChain::GetBackBufferResource
	};
}