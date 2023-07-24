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
			// Note: We do not maintain a Sampler descriptor heap

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
			// TODO: Combine hashes, instead of nesting hash tables

			// Hashed D3D12_VERSIONED_ROOT_SIGNATURE_DESC -> Root sig object
			std::unordered_map<uint64_t, std::shared_ptr<dx12::RootSignature>> m_rootSigLibrary;
			
			std::vector<dx12::CPUDescriptorHeapManager> m_cpuDescriptorHeapMgrs; // CPUDescriptorHeapType_Count

			// Imgui descriptor heap: A single, CPU and GPU-visible SRV descriptor for the internal font texture
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imGuiGPUVisibleSRVDescriptorHeap;
		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static std::shared_ptr<dx12::PipelineState> CreateAddPipelineState(
			re::Shader const&, gr::PipelineState&, re::TextureTargetSet const&);
		
		// TODO: Move these to the system info layer:
		static uint8_t GetMaxTextureInputs();


		// DX12-specific interface:
		static dx12::CommandQueue& GetCommandQueue(CommandList::CommandListType type);
		static dx12::CommandQueue& GetCommandQueue(uint64_t fenceValue); // Get the command queue that produced a fence value

		static dx12::GlobalResourceStateTracker& GetGlobalResourceStateTracker();
		
		static std::shared_ptr<dx12::PipelineState> GetPipelineStateObject(
			re::Shader const& shader,
			gr::PipelineState& grPipelineState,
			re::TextureTargetSet const* targetSet); // Null targetSet is valid (indicates the backbuffer)

		static bool HasRootSignature(uint64_t rootSigDescHash);
		static std::shared_ptr<dx12::RootSignature> GetRootSignature(uint64_t rootSigDescHash);
		static void AddRootSignature(std::shared_ptr<dx12::RootSignature>);
		
		// Static null descriptor library
		// TODO: Switch to inheritance and move these inside of the dx12::Context object
		static constexpr D3D12_CONSTANT_BUFFER_VIEW_DESC m_nullCBV = { .BufferLocation = 0, .SizeInBytes = 32 }; // Arbitrary

		static DescriptorAllocation const& GetNullSRVDescriptor(D3D12_SRV_DIMENSION, DXGI_FORMAT);
		static std::unordered_map<D3D12_SRV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullSRVLibrary;
		static std::mutex s_nullSRVLibraryMutex;

		static DescriptorAllocation const& GetNullUAVDescriptor(D3D12_UAV_DIMENSION, DXGI_FORMAT);
		static std::unordered_map<D3D12_UAV_DIMENSION, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>> s_nullUAVLibrary;
		static std::mutex s_nullUAVLibraryMutex;
	};
}