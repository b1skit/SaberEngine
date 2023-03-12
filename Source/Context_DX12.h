// © 2022 Adam Badke. All rights reserved.
#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "CPUDescriptorHeapManager_DX12.h"
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
		enum DescriptorHeapType
		{
			CBV_SRV_UAV,
			Sampler,
			RTV,
			DSV,

			DescriptorHeapType_Count
		};


	public:
		struct PlatformParams final : public re::Context::PlatformParams
		{
			PlatformParams();

			dx12::Device m_device;

			std::array<dx12::CommandQueue, CommandList::CommandListType::CommandListType_Count> m_commandQueues;

			uint64_t m_frameFenceValues[dx12::RenderManager::k_numFrames]; // Fence values for signalling the command queue
			
			// Last fence value signalled from the current frame's command lists. Populated at the end of 
			// dx12::RenderManager::Render, and used to insert a GPU wait in dx12::Context::Present
			uint64_t m_lastFenceBeforePresent = 0;

			// TODO: Precompute a library of all pipeline states needed at startup. For now, we just have a single PSO
			std::shared_ptr<dx12::PipelineState> m_pipelineState;

			std::vector<dx12::CPUDescriptorHeapManager> m_descriptorHeapMgrs; // DescriptorHeapType_Count
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static std::shared_ptr<dx12::PipelineState> CreateAddPipelineState(
			gr::PipelineState const&, re::Shader const&, D3D12_RT_FORMAT_ARRAY const& rtvFormats, const DXGI_FORMAT dsvFormat);
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static dx12::CommandQueue& GetCommandQueue(CommandList::CommandListType type);

		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
			Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

		// TODO:
		// Add a helper wrapper to get:
		// - The current backbuffer index from the swapchain: dx12::SwapChain::GetBackBufferIdx
		// - The swapchain backbuffer resource: dx12::SwapChain::GetBackBufferResource
	};
}