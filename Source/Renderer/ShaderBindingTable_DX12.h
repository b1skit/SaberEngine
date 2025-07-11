// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "HeapManager_DX12.h"
#include "ShaderBindingTable.h"


namespace re
{
	struct ASInput;
	class BindlessResourceManager;
	class BufferInput;
	struct RWTextureInput;
	struct TextureAndSamplerInput;
}

namespace dx12
{
	class CommandList;
	class GPUDescriptorHeap;


	class ShaderBindingTable
	{
	public:
		struct PlatObj final : public re::ShaderBindingTable::PlatObj
		{
			void Destroy() override;

			// We allocate enough memory for N frames-in-flight-worth of SBT data, and index into it each frame using 
			// the current frame number.
			// Note: The HeapManager's deferred delete will (unnecessarily) keep this alive for an additional N frames
			// in flight after the IPlatObj deferred delete happens
			std::unique_ptr<GPUResource> m_SBT;
			
			// Relative offsets and strides within m_SBT (i.e. from the base offset of the current frame):
			uint32_t m_rayGenRegionBaseOffset = 0;
			uint32_t m_rayGenRegionByteStride = 0;
			uint32_t m_rayGenRegionTotalByteSize = 0;

			uint32_t m_missRegionBaseOffset = 0;
			uint32_t m_missRegionByteStride = 0;
			uint32_t m_missRegionTotalByteSize = 0;

			uint32_t m_hitGroupRegionBaseOffset = 0;
			uint32_t m_hitGroupRegionByteStride = 0;
			uint32_t m_hitGroupRegionTotalByteSize = 0;

			uint32_t m_callableRegionBaseOffset = 0;
			uint32_t m_callableRegionByteStride = 0;
			uint32_t m_callableRegionTotalByteSize = 0;

			// Ray tracing pipeline state:
			Microsoft::WRL::ComPtr<ID3D12StateObject> m_rayTracingStateObject;
			Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_rayTracingStateObjectProperties;

			// Track the per-frame SBT partitioning:
			uint64_t m_frameRegionByteSize;
			uint8_t m_numFramesInFlight;
		};

	public: // Platform functionality:
		static void Create(re::ShaderBindingTable&, uint8_t numFramesInFlight);


	public: // DX12-specific functionality (to be called from dx12::CommandList)
		static void SetTLASOnLocalRoots(
			re::ShaderBindingTable const&,
			re::ASInput const&,
			dx12::GPUDescriptorHeap*,
			uint64_t currentFrameNum);

		static void SetTexturesOnLocalRoots(
			re::ShaderBindingTable const&,
			std::vector<re::TextureAndSamplerInput> const&,
			dx12::CommandList*,
			dx12::GPUDescriptorHeap*,
			uint64_t currentFrameNum);
		
		static void SetBuffersOnLocalRoots(
			re::ShaderBindingTable const&, 
			std::vector<re::BufferInput> const&,
			dx12::CommandList*,
			dx12::GPUDescriptorHeap*,
			uint64_t currentFrameNum);

		static void SetRWTextureOnLocalRoots(
			re::ShaderBindingTable const&,
			re::RWTextureInput const&,
			dx12::GPUDescriptorHeap*,
			uint64_t currentFrameNum);

		static D3D12_DISPATCH_RAYS_DESC BuildDispatchRaysDesc(
			re::ShaderBindingTable const&,
			glm::uvec3 const& threadDimensions,
			uint64_t currentFrameNum,
			uint32_t rayGenShaderIdx);
	};
}