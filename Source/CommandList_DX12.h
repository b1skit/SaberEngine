// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "PipelineState_DX12.h"
#include "ResourceStateTracker_DX12.h"

#include <wrl.h>
#include <d3d12.h>

namespace re
{
	class Batch;
	class Buffer;
	class VertexStream;
	class Texture;
	class TextureTarget;
	class TextureTargetSet;
}

namespace dx12
{
	class RootSignature;
	class PipelineState;

	enum CommandListType
	{
		Direct,
		Bundle,
		Compute,
		Copy,

		CommandListType_Count,
		CommandListType_Invalid = CommandListType_Count
	};
	static_assert(CommandListType_Count <= 7); // We pack command list type into the upper 3 bits of fence values


	class CommandList
	{
	public:
		static constexpr wchar_t const* const GetCommandListTypeWName(dx12::CommandListType);
		static constexpr char const* const GetCommandListTypeName(dx12::CommandListType);
		static constexpr D3D12_COMMAND_LIST_TYPE TranslateToD3DCommandListType(dx12::CommandListType);
		static constexpr CommandListType TranslateToSECommandListType(D3D12_COMMAND_LIST_TYPE);

		static size_t s_commandListNumber; // Monotonically-increasing numeric ID for naming command lists


	public:
		CommandList(ID3D12Device2*, CommandListType);
		CommandList(CommandList&&) = default;
		CommandList& operator=(CommandList&&) = default;
		~CommandList() { Destroy(); }

		void Destroy();

		// The reuse fence tracks if the the last work the command list recorded/submitted been completed
		uint64_t GetReuseFenceValue() const;
		void SetReuseFenceValue(uint64_t);

		void Reset();
		void Close() const;

		// The pipeline state and root signature must be set before subsequent interactions with the command list
		void SetPipelineState(dx12::PipelineState const&);

		void SetGraphicsRootSignature(dx12::RootSignature const* rootSig); // Makes all descriptors stale
		void SetComputeRootSignature(dx12::RootSignature const* rootSig); // Makes all descriptors stale

		// GPU descriptors:
		void CommitGPUDescriptors(); // Must be called before issuing draw commands
		void SetBuffer(re::Buffer const*);

		void SetGraphicsRoot32BitConstants(
			uint32_t rootParamIdx, uint32_t count, void const* srcData, uint32_t dstOffset) const;

		void SetTexture(
			std::string const& shaderName, re::Texture const*, uint32_t srcMip, bool skipTransition);

		void SetRenderTargets(re::TextureTargetSet const&, bool readOnlyDepth);
		void SetComputeTargets(re::TextureTargetSet const&);

		void ClearDepthTarget(re::TextureTarget const*);

		void ClearColorTarget(re::TextureTarget const*);
		void ClearColorTargets(re::TextureTargetSet const&);

		void ClearTargets(re::TextureTargetSet const&);

		void SetViewport(re::TextureTargetSet const&) const;
		void SetScissorRect(re::TextureTargetSet const&) const;
		
		void DrawBatchGeometry(re::Batch const&);

		void Dispatch(glm::uvec3 const& numThreads);

		void UpdateSubresources(re::Texture const*, ID3D12Resource* intermediate, size_t intermediateOffset);
		void UpdateSubresources(re::VertexStream const*, ID3D12Resource* intermediate, size_t intermediateOffset);
		void UpdateSubresources(
			re::Buffer const*, uint32_t dstOffset, ID3D12Resource* srcResource, uint32_t srcOffset, uint32_t numBytes);

		void CopyResource(ID3D12Resource* srcResource, ID3D12Resource* dstResource);

		void TransitionResource(
			ID3D12Resource*, uint32_t totalSubresources, D3D12_RESOURCE_STATES to, uint32_t targetSubresource);

		void TransitionResource(re::Texture const*, D3D12_RESOURCE_STATES to, uint32_t mipLevel);

		void ResourceBarrier(uint32_t numBarriers, D3D12_RESOURCE_BARRIER const* barriers);

		CommandListType GetCommandListType() const;
		ID3D12GraphicsCommandList2* GetD3DCommandList() const;

		LocalResourceStateTracker const& GetLocalResourceStates() const;

		void DebugPrintResourceStates() const;


	private:
		void InsertUAVBarrier(ID3D12Resource*);
		void InsertUAVBarrier(std::shared_ptr<re::Texture>);

		void SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY) const;

		void SetVertexBuffer(uint32_t slot, re::VertexStream const*);
		void SetVertexBuffers(re::VertexStream const* const* streams, uint8_t count);

		void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW*) const;

		void TransitionResourceInternal(
			ID3D12Resource*, 
			uint32_t totalSubresources, 
			D3D12_RESOURCE_STATES to, 
			uint32_t targetSubresource, 
			uint32_t numFaces, 
			uint32_t numMips);

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;

		CommandListType m_type;
		D3D12_COMMAND_LIST_TYPE m_d3dType;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		uint64_t m_commandAllocatorReuseFenceValue; // When the command allocator can be reused

		const size_t k_commandListNumber; // Monotonically increasing identifier assigned at creation


	private:
		// The D3D docs recommend using a single GPU-visible heap of each type (CBV/SRV/UAV or SAMPLER), and setting it
		// once per frame, as changing descriptor heaps can cause pipeline flushes on some hardware
		std::unique_ptr<dx12::GPUDescriptorHeap> m_gpuCbvSrvUavDescriptorHeaps;
		dx12::LocalResourceStateTracker m_resourceStates;


	public:
		struct ReadbackResourceMetadata
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


	inline void CommandList::SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW* view) const
	{
		m_commandList->IASetIndexBuffer(view);
	}


	inline void CommandList::SetGraphicsRoot32BitConstants(
		uint32_t rootParamIdx, uint32_t count, void const* srcData, uint32_t dstOffset) const
	{
		m_commandList->SetGraphicsRoot32BitConstants(
			rootParamIdx,	// RootParameterIndex (As set in our CD3DX12_ROOT_PARAMETER1)
			count,			// Num32BitValuesToSet
			srcData,		// pSrcData
			dstOffset);
	}


	inline void CommandList::Dispatch(glm::uvec3 const& numThreads)
	{
		SEAssert(numThreads.x < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.y < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.z < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION,
			"Invalid dispatch dimensions");

		CommitGPUDescriptors();

		m_commandList->Dispatch(numThreads.x, numThreads.y, numThreads.z);
	}


	inline CommandListType CommandList::GetCommandListType() const
	{
		return m_type;
	}


	inline ID3D12GraphicsCommandList2* CommandList::GetD3DCommandList() const
	{
		return m_commandList.Get();
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
		m_gpuCbvSrvUavDescriptorHeaps->Commit();
	}


	inline std::vector<CommandList::ReadbackResourceMetadata> const& CommandList::GetReadbackResources() const
	{
		return m_seenReadbackResources;
	}
}