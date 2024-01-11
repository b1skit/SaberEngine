// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "Debug_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "PipelineState_DX12.h"
#include "ResourceStateTracker_DX12.h"


namespace re
{
	class Batch;
	class ParameterBlock;
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
		VideoDecode,
		VideoProcess,
		VideoEncode,

		CommandListType_Count,
		CommandListType_Invalid = CommandListType_Count
	};
	static_assert(CommandListType_Count == 7); // We pack command list type into the upper bits of fence values


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
		void SetParameterBlock(re::ParameterBlock const*);

		void SetGraphicsRoot32BitConstants(
			uint32_t rootParamIdx, uint32_t count, void const* srcData, uint32_t dstOffset) const;

		void SetTexture(
			std::string const& shaderName, re::Texture const*, uint32_t srcMip, bool skipTransition);

		void SetRenderTargets(re::TextureTargetSet const&, bool readOnlyDepth);
		void SetComputeTargets(re::TextureTargetSet const&);

		void ClearDepthTarget(re::TextureTarget const*) const;

		void ClearColorTarget(re::TextureTarget const*) const;
		void ClearColorTargets(re::TextureTargetSet const&) const;

		void SetViewport(re::TextureTargetSet const&) const;
		void SetScissorRect(re::TextureTargetSet const&) const;
		
		void DrawBatchGeometry(re::Batch const&);

		void Dispatch(glm::uvec3 const& numThreads);

		void UpdateSubresources(re::Texture const*, ID3D12Resource* intermediate, size_t intermediateOffset);
		void UpdateSubresources(re::VertexStream const*, ID3D12Resource* intermediate, size_t intermediateOffset);

		// TODO: Implement a "resource" interface if/when we need to transition more than just Textures
		void TransitionResource(re::Texture const*, D3D12_RESOURCE_STATES to, uint32_t mipLevel);
		void ResourceBarrier(uint32_t numBarriers, D3D12_RESOURCE_BARRIER const* barriers);

		CommandListType GetCommandListType() const;
		ID3D12GraphicsCommandList2* GetD3DCommandList() const;

		LocalResourceStateTracker const& GetLocalResourceStates() const;

		void DebugPrintResourceStates() const;


	private:
		void InsertUAVBarrier(std::shared_ptr<re::Texture>);

		void SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY) const;

		void SetVertexBuffer(uint32_t slot, re::VertexStream const*) const;
		void SetVertexBuffers(re::VertexStream const* const* streams, uint8_t count) const;

		void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW*) const;


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


	private:
		// Note: These cached pointers could be graphics OR compute-specific
		dx12::RootSignature const* m_currentRootSignature;
		dx12::PipelineState const* m_currentPSO;


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
		SEAssert("Invalid dispatch dimensions",
			numThreads.x < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.y < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.z < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION);

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
		case CommandListType::VideoDecode: return L"VideoDecode";
		case CommandListType::VideoProcess: return L"VideoProcess";
		case CommandListType::VideoEncode: return L"VideoEncode";
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
		case CommandListType::VideoDecode: return "VideoDecode";
		case CommandListType::VideoProcess: return "VideoProcess";
		case CommandListType::VideoEncode: return "VideoEncode";
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
		case dx12::CommandListType::VideoDecode:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
		case dx12::CommandListType::VideoProcess:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
		case dx12::CommandListType::VideoEncode:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;
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
}