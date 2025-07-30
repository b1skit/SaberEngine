// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"
#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "RasterState.h"
#include "ResourceStateTracker_DX12.h"

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
#include "Core/Logger.h"
#endif


namespace re
{
	class AccelerationStructure;
	class BindlessResourceManager;
	class Buffer;
	class BufferInput;
	class Texture;
	class TextureTarget;
	class TextureTargetSet;
	class RootConstants;
	class ShaderBindingTable;
	class VertexBufferInput;
	class VertexStream;

	struct ASInput;
	struct RWTextureInput;
	struct TextureAndSamplerInput;
	struct TextureView;
}

namespace dx12
{
	class Context;
	class GPUDescriptorHeap;
	class RootSignature;
	class PipelineState;

	enum CommandListType : uint8_t
	{
		Direct,
		Bundle,
		Compute,
		Copy,

		CommandListType_Count,
		CommandListType_Invalid = CommandListType_Count
	};
	static_assert(CommandListType_Count <= 7); // We pack command list type into the upper 3 bits of fence values


	class CommandList final
	{
	public:
		static constexpr wchar_t const* const GetCommandListTypeWName(dx12::CommandListType);
		static constexpr char const* const GetCommandListTypeName(dx12::CommandListType);
		static constexpr D3D12_COMMAND_LIST_TYPE TranslateToD3DCommandListType(dx12::CommandListType);
		static constexpr CommandListType TranslateToSECommandListType(D3D12_COMMAND_LIST_TYPE);

		static size_t s_commandListNumber; // Monotonically-increasing numeric ID for naming command lists


	public:
		struct TransitionMetadata final
		{
			ID3D12Resource* m_resource;
			D3D12_RESOURCE_STATES m_toState;
			std::vector<uint32_t> m_subresourceIndexes;
		};

	public:
		// Arbitrary: Total descriptors in our local GPU-visible descriptor heap
		static constexpr uint32_t k_gpuDescriptorHeapSize = 4096;


	public:
		CommandList(dx12::Context*, CommandListType);
		CommandList(CommandList&&) noexcept = default;
		CommandList& operator=(CommandList&&) noexcept = default;
		~CommandList() { Destroy(); }

		void Destroy();

		// The reuse fence tracks if the the last work the command list recorded/submitted been completed
		uint64_t GetReuseFenceValue() const;
		void SetReuseFenceValue(uint64_t);

		void Reset();
		void Close() const;

		// The pipeline state and root signature must be set before subsequent interactions with the command list
		void SetPipelineState(dx12::PipelineState const&);

		void SetGraphicsRootSignature(dx12::RootSignature const*); // Makes all descriptors stale
		void SetComputeRootSignature(dx12::RootSignature const*); // Makes all descriptors stale

		void SetRootConstants(re::RootConstants const&) const;

		void SetRenderTargets(re::TextureTargetSet const&);
		
		void ClearColorTargets(
			bool const* clearModes,
			glm::vec4 const* colorClearVals,
			uint8_t numColorClears, 
			re::TextureTargetSet const&);

		void ClearTargets(
			bool const* colorClearModes,
			glm::vec4 const* colorClearVals,
			uint8_t numColorClears,
			bool depthClearMode,
			float depthClearVal,
			bool stencilClearMode,
			uint8_t stencilClearVal,
			re::TextureTargetSet const&);

		void ClearDepthStencilTarget(
			bool depthClearMode,
			float depthClearVal,
			bool stencilClearMode,
			uint8_t stencilClearVal,
			re::TextureTarget const&);

		void ClearUAV(std::vector<re::RWTextureInput> const&, glm::vec4 const& clearVal);
		void ClearUAV(std::vector<re::RWTextureInput> const&, glm::uvec4 const& clearVal);

		void SetViewport(re::TextureTargetSet const&) const;
		void SetScissorRect(re::TextureTargetSet const&) const;

		void SetTextures(std::vector<re::TextureAndSamplerInput> const&, int depthTargetTexInputIdx = -1);
		void SetTextures(
			std::vector<re::TextureAndSamplerInput> const&, re::ShaderBindingTable const&, uint64_t currentFrameNum);

		void SetBuffers(std::vector<re::BufferInput> const&);

		void SetRWTextures(std::vector<re::RWTextureInput> const&);

		void SetTLAS(re::ASInput const&); // For inline ray tracing
		void BuildRaytracingAccelerationStructure(re::AccelerationStructure&, bool doUpdate);

		void AttachBindlessResources(
			re::ShaderBindingTable const&, re::BindlessResourceManager const&, uint64_t currentFrameNum);

		void DrawGeometry(
			re::RasterState::PrimitiveTopology,
			re::GeometryMode, 
			std::array<std::pair<re::VertexBufferInput const*, uint8_t>, re::VertexStream::k_maxVertexStreams> const&,
			re::VertexBufferInput const&,
			uint32_t instanceCount);
		void Dispatch(glm::uvec3 const& threadDimensions);
		void DispatchRays(
			re::ShaderBindingTable const&, 
			glm::uvec3 const& threadDimensions,
			uint32_t rayGenShaderIdx,
			uint64_t currentFrameNum);

		void UpdateSubresource(
			core::InvPtr<re::Texture> const&,
			uint32_t arrayIdx,
			uint32_t faceIdx,
			uint32_t mipLevel,
			ID3D12Resource* intermediate,
			size_t intermediateOffset);

		void UpdateSubresources(
			re::Buffer const*, uint32_t dstOffset, ID3D12Resource* srcResource, uint64_t srcOffset, uint64_t numBytes);

		void CopyResource(ID3D12Resource* srcResource, ID3D12Resource* dstResource);
		void CopyTexture(core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst);

		void TransitionResource(core::InvPtr<re::Texture> const&, D3D12_RESOURCE_STATES to, re::TextureView const&);
		void TransitionResources(std::vector<TransitionMetadata>&&);

		void ResourceBarrier(uint32_t numBarriers, D3D12_RESOURCE_BARRIER const* barriers);

		CommandListType GetCommandListType() const;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> const& GetD3DCommandList() const;

		LocalResourceStateTracker const& GetLocalResourceStates() const;

		void DebugPrintResourceStates() const;


	private:
		void CommitGPUDescriptors(); // GPU descriptors: Must be called before issuing draw commands

		void InsertUAVBarrier(ID3D12Resource*);
		void InsertUAVBarrier(core::InvPtr<re::Texture> const&);

		void SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY) const;

		void SetVertexBuffers(std::array<std::pair<re::VertexBufferInput const*, uint8_t>, re::VertexStream::k_maxVertexStreams> const&);

		void SetIndexBuffer(re::VertexBufferInput const&);

		void TransitionResourceInternal(
			ID3D12Resource*,
			D3D12_RESOURCE_STATES to,
			std::vector<uint32_t>&& subresourceIndexes);

		void TransitionResourcesInternal(std::vector<TransitionMetadata>&&);


	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
		
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		uint64_t m_commandAllocatorReuseFenceValue; // When the command allocator can be reused

		dx12::Context* m_context;
		ID3D12Device* m_device; // Cached for convenience

		const size_t k_commandListNumber; // Monotonically increasing identifier assigned at creation

		D3D12_COMMAND_LIST_TYPE m_d3dType;
		CommandListType m_type;


	private:
		dx12::LocalResourceStateTracker m_resourceStates;

		// The D3D docs recommend using a single GPU-visible heap of each type (CBV/SRV/UAV or SAMPLER), and setting it
		// once per frame, as changing descriptor heaps can cause pipeline flushes on some hardware
		std::unique_ptr<dx12::GPUDescriptorHeap> m_gpuCbvSrvUavDescriptorHeap;

		enum class DescriptorHeapSource : uint8_t
		{
			Own,		// i.e. m_gpuCbvSrvUavDescriptorHeap
			External,	// e.g. BindlessResourceManager

			Unset,
		} m_currentDescriptorHeapSource;
		void SetDescriptorHeap(ID3D12DescriptorHeap*);


	public:
		struct ReadbackResourceMetadata final
		{
			ID3D12Resource* m_srcResource;
			ID3D12Resource* m_dstResource;
			uint64_t* m_dstModificationFence;
			std::mutex* m_dstModificationFenceMutex;
		};
		std::vector<ReadbackResourceMetadata> const& GetReadbackResources() const;

	private:
		// Track any readback resources encountered during recording, so we can schedule copies when we're done
		std::vector<ReadbackResourceMetadata> m_seenReadbackResources;


	private:
		// Note: These cached pointers could be graphics OR compute-specific
		dx12::RootSignature const* m_currentRootSignature;
		dx12::PipelineState const* m_currentPSO;


		// DEBUG:
#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
	public:
		void RecordStageName(std::string const& name) { m_debugRecordedStages.emplace_back(name); }

	private:
		std::vector<std::string> m_debugRecordedStages; // The stages this command list was used on for the frame
#endif


	private: // No copying allowed
		CommandList() = delete;
		CommandList(CommandList const&) = delete;
		CommandList& operator=(CommandList const&) = delete;
	};


	inline uint64_t CommandList::GetReuseFenceValue() const
	{
		return m_commandAllocatorReuseFenceValue;
	}


	inline void CommandList::SetReuseFenceValue(uint64_t fenceValue)
	{
		m_commandAllocatorReuseFenceValue = fenceValue;
	}


	inline void CommandList::Close() const
	{
		HRESULT hr = m_commandList->Close();
		CheckHResult(hr, "Failed to close command list");

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
		std::string stagesNames;
		for (auto const& stage : m_debugRecordedStages)
		{
			stagesNames += stage + ", ";
		}

		LOG_WARNING(std::format("{} recorded stages: {}", 
			dx12::GetDebugName(m_commandList.Get()),
			stagesNames).c_str());
#endif
	}


	inline void CommandList::SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY topologyType) const
	{
		m_commandList->IASetPrimitiveTopology(topologyType);
	}


	inline void CommandList::TransitionResources(std::vector<TransitionMetadata>&& resourceTransitions)
	{
		TransitionResourcesInternal(std::move(resourceTransitions));
	}


	inline CommandListType CommandList::GetCommandListType() const
	{
		return m_type;
	}


	inline Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> const& CommandList::GetD3DCommandList() const
	{
		return m_commandList;
	}


	constexpr wchar_t const* const CommandList::GetCommandListTypeWName(dx12::CommandListType type)
	{
		switch (type)
		{
		case CommandListType::Direct: return L"Direct";
		case CommandListType::Bundle: return L"Bundle";
		case CommandListType::Compute: return L"Compute";
		case CommandListType::Copy: return L"Copy";
		default:
			static_assert("Invalid command list type");
		}
		return L"InvalidType";
	};


	constexpr char const* const CommandList::GetCommandListTypeName(dx12::CommandListType type)
	{
		switch (type)
		{
		case CommandListType::Direct: return "Direct";
		case CommandListType::Bundle: return "Bundle";
		case CommandListType::Compute: return "Compute";
		case CommandListType::Copy: return "Copy";
		default:
			static_assert("Invalid command list type");
		}
		return "InvalidType";
	};


	constexpr D3D12_COMMAND_LIST_TYPE CommandList::TranslateToD3DCommandListType(dx12::CommandListType type)
	{
		switch (type)
		{
		case dx12::CommandListType::Direct:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
		case dx12::CommandListType::Bundle:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_BUNDLE;
		case dx12::CommandListType::Compute:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case dx12::CommandListType::Copy:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY;
		case dx12::CommandListType::CommandListType_Count:
		default:
			static_assert("Invalid type");
		}

		return static_cast<D3D12_COMMAND_LIST_TYPE>(-1); // D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_NONE
	}


	inline void CommandList::CommitGPUDescriptors()
	{
		if (m_currentDescriptorHeapSource != DescriptorHeapSource::Own)
		{
			SetDescriptorHeap(m_gpuCbvSrvUavDescriptorHeap->GetD3DDescriptorHeap());
		}

		m_gpuCbvSrvUavDescriptorHeap->Commit(*this);
	}


	inline std::vector<CommandList::ReadbackResourceMetadata> const& CommandList::GetReadbackResources() const
	{
		return m_seenReadbackResources;
	}


	inline LocalResourceStateTracker const& CommandList::GetLocalResourceStates() const
	{
		return m_resourceStates;
	}
}