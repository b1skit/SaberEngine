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
	class ParameterBlock;
	class VertexStream;
	class TextureTarget;
	class TextureTargetSet;
}

namespace dx12
{
	class RootSignature;
	class PipelineState;

	class CommandList
	{
	public:
		enum CommandListType
		{
			Direct,
			Bundle,
			Compute,
			Copy,
			VideoDecode,
			VideoProcess,
			VideoEncode,

			CommandListType_Count
		};

		static constexpr D3D12_COMMAND_LIST_TYPE D3DCommandListType(dx12::CommandList::CommandListType type);


	public:
		CommandList(ID3D12Device2*, D3D12_COMMAND_LIST_TYPE);
		CommandList(CommandList&&) = default;
		CommandList& operator=(CommandList&&) = default;
		~CommandList() { Destroy(); }

		void Destroy();

		uint64_t GetFenceValue() const;
		void SetFenceValue(uint64_t);

		void Reset();
		void Close() const;

		// The pipeline state and root signature must be set before subsequent interactions with the command list
		void SetPipelineState(dx12::PipelineState const&);
		void SetGraphicsRootSignature(dx12::RootSignature const& rootSig); // Makes all descriptors stale
		// TODO: void SetComputeRootSignature(dx12::RootSignature const& rootSig);

		// GPU descriptors:
		void SetParameterBlock(re::ParameterBlock const*);
		void CommitGPUDescriptors(); // Must be called before issuing draw commands

		// TODO: Write a helper that takes a MeshPrimitive; make these private
		void SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY) const;
		
		void SetVertexBuffer(uint32_t slot, re::VertexStream const*) const;
		void SetVertexBuffers(std::vector<std::shared_ptr<re::VertexStream>> const&) const;
		
		void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW*) const;

		void ClearDepthTarget(re::TextureTarget const&) const; // Uses target texture's R clear color
		void ClearDepthTarget(re::TextureTarget const&, float clearColor) const;

		void ClearColorTarget(re::TextureTarget const&) const;
		void ClearColorTarget(re::TextureTarget const&, glm::vec4 clearColor) const;
		void ClearColorTargets(re::TextureTargetSet const&) const;

		void SetRenderTargets(re::TextureTargetSet&) const;
		void SetBackbufferRenderTarget() const;

		void SetViewport(re::TextureTargetSet const&) const;
		void SetScissorRect(re::TextureTargetSet const&) const;

		void SetGraphicsRoot32BitConstants(
			uint32_t rootParamIdx, uint32_t count, void const* srcData, uint32_t dstOffset) const;

		void DrawIndexedInstanced(
			uint32_t numIndexes, uint32_t numInstances, uint32_t idxStartOffset, int32_t baseVertexOffset, uint32_t instanceOffset);

		void TransitionResource(ID3D12Resource* resources, D3D12_RESOURCE_STATES to, uint32_t subresourceIdx);

		D3D12_COMMAND_LIST_TYPE GetType() const;
		ID3D12GraphicsCommandList2* GetD3DCommandList() const;

		LocalResourceStateTracker const& GetLocalResourceStates() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;
		D3D12_COMMAND_LIST_TYPE m_type;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		uint64_t m_fenceValue; // When the command allocator can be reused


	private:
		std::unique_ptr<dx12::GPUDescriptorHeap> m_gpuDescriptorHeaps;
		dx12::LocalResourceStateTracker m_resourceStates;

	private:
		dx12::RootSignature const* m_currentGraphicsRootSignature;
		dx12::PipelineState const* m_currentPSO;


	private: // No copying allowed
		CommandList() = delete;
		CommandList(CommandList const&) = delete;
		CommandList& operator=(CommandList const&) = delete;
	};


	inline uint64_t CommandList::GetFenceValue() const
	{
		return m_fenceValue;
	}


	inline void CommandList::SetFenceValue(uint64_t fenceValue)
	{
		m_fenceValue = fenceValue;
	}


	inline void CommandList::Reset()
	{
		m_currentGraphicsRootSignature = nullptr;
		m_currentPSO = nullptr;

		// Reset the command allocator BEFORE we reset the command list (to avoid leaking memory)
		m_commandAllocator->Reset();

		m_resourceStates.Reset();

		// Note: pso is optional here; nullptr sets a dummy PSO
		HRESULT hr = m_commandList->Reset(m_commandAllocator.Get(), nullptr);   
		CheckHResult(hr, "Failed to reset command list");

		// Re-bind the descriptor heaps (unless we're a copy command list):
		if (m_type != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// TODO: Handle sampler descriptor heaps

			ID3D12DescriptorHeap* descriptorHeaps[1] = { m_gpuDescriptorHeaps->GetD3DDescriptorHeap() };
			m_commandList->SetDescriptorHeaps(1, descriptorHeaps);
		}
	}

	inline void CommandList::Close() const
	{
		HRESULT hr = m_commandList->Close();
		CheckHResult(hr, "Failed to close command list");
	}


	inline void CommandList::SetPipelineState(dx12::PipelineState const& pso)
	{
		m_currentPSO = &pso;

		m_commandList->SetPipelineState(pso.GetD3DPipelineState());
	}


	inline void CommandList::SetGraphicsRootSignature(dx12::RootSignature const& rootSig)
	{
		m_currentGraphicsRootSignature = &rootSig;

		m_gpuDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		m_commandList->SetGraphicsRootSignature(rootSig.GetD3DRootSignature());
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
		// TODO: Add an assert: Is the root param index free?

		m_commandList->SetGraphicsRoot32BitConstants(
			rootParamIdx,	// RootParameterIndex (As set in our CD3DX12_ROOT_PARAMETER1)
			count,			// Num32BitValuesToSet
			srcData,		// pSrcData
			dstOffset);
	}


	inline void CommandList::DrawIndexedInstanced(
		uint32_t numIndexes, uint32_t numInstances, uint32_t idxStartOffset, int32_t baseVertexOffset, uint32_t instanceOffset)
	{
		CommitGPUDescriptors();

		m_commandList->DrawIndexedInstanced(
			numIndexes,			// Index count, per instance
			numInstances,		// Instance count
			idxStartOffset,		// Start index location
			baseVertexOffset,	// Base vertex location
			instanceOffset);	// Start instance location
	}


	inline D3D12_COMMAND_LIST_TYPE CommandList::GetType() const
	{
		return m_type;
	}


	inline ID3D12GraphicsCommandList2* CommandList::GetD3DCommandList() const
	{
		return m_commandList.Get();
	}


	constexpr D3D12_COMMAND_LIST_TYPE CommandList::D3DCommandListType(dx12::CommandList::CommandListType type)
	{
		switch (type)
		{
		case dx12::CommandList::CommandListType::Direct:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
		case dx12::CommandList::CommandListType::Bundle:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_BUNDLE;
		case dx12::CommandList::CommandListType::Compute:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case dx12::CommandList::CommandListType::Copy:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY;
		case dx12::CommandList::CommandListType::VideoDecode:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE;
		case dx12::CommandList::CommandListType::VideoProcess:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
		case dx12::CommandList::CommandListType::VideoEncode:
			return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;
		case dx12::CommandList::CommandListType::CommandListType_Count:
		default:
			static_assert("Invalid type");
		}

		return D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT;
	}


	inline void CommandList::CommitGPUDescriptors()
	{
		m_gpuDescriptorHeaps->Commit();
	}
}