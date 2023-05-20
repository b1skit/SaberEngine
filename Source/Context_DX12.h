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
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	class Context
	{
	public:
		enum CPUDescriptorHeapType
		{
			CBV_SRV_UAV,
			Sampler,

			// These types cannot be used with D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
			RTV,
			DSV,

			CPUDescriptorHeapType_Count
		};


	public:
		struct PlatformParams final : public re::Context::PlatformParams
		{
			PlatformParams();

			dx12::Device m_device;

			// TODO: We should template the CommandQueue types, to allow specific helper functions on each type
			std::array<dx12::CommandQueue, CommandList::CommandListType::CommandListType_Count> m_commandQueues;

			dx12::GlobalResourceStateTracker m_globalResourceStates;

			uint64_t m_frameFenceValues[dx12::RenderManager::k_numFrames]; // Fence values for signalling the command queue
			
			// Access the PSO library via dx12::Context::GetPipelineStateObject():
			std::unordered_map<uint64_t, // re::Shader::GetName()
				std::unordered_map<uint64_t, // gr::PipelineState::GetPipelineStateDataHash()
					std::unordered_map<uint64_t, // re::TextureTargetSet::GetTargetSetSignature()
						std::shared_ptr<dx12::PipelineState>>>> m_PSOLibrary;
			
			std::vector<dx12::CPUDescriptorHeapManager> m_cpuDescriptorHeapMgrs; // CPUDescriptorHeapType_Count

			// Imgui descriptor heap: A single, CPU and GPU-visible SRV descriptor for the internal font texture
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiGPUVisibleSRVDescriptorHeap;
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void CreateAddPipelineState(re::Shader const&, gr::PipelineState&, re::TextureTargetSet const&);
		
		// TODO: Move these to the system info layer:
		static uint8_t GetMaxTextureInputs();
		static uint8_t GetMaxColorTargets();


		// DX12-specific interface:
		static dx12::CommandQueue& GetCommandQueue(CommandList::CommandListType type);

		static dx12::GlobalResourceStateTracker& GetGlobalResourceStateTracker();
		
		static std::shared_ptr<dx12::PipelineState> GetPipelineStateObject(
			re::Shader const& shader,
			gr::PipelineState& grPipelineState,
			re::TextureTargetSet const* targetSet); // Null targetSet is valid (indicates the backbuffer)

		
	};
}