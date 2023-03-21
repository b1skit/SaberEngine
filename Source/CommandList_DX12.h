// � 2022 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "GPUDescriptorHeap_DX12.h"

struct CD3DX12_CPU_DESCRIPTOR_HANDLE;
struct D3D12_CPU_DESCRIPTOR_HANDLE;


namespace dx12
{
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
		~CommandList() { Destroy(); }

		void Destroy();

		bool GetFenceValue() const;
		void SetFenceValue(uint64_t);

		void Reset(ID3D12PipelineState*) const;
		void Close() const;

		void SetPipelineState(ID3D12PipelineState*) const;
		void SetGraphicsRootSignature(dx12::RootSignature const& rootSig);
		// TODO: void SetComputeRootSignature(dx12::RootSignature const& rootSig);

		// TODO: Write a helper that takes a MeshPrimitive; make these private
		void SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY) const;
		void SetVertexBuffers(uint32_t startSlot, uint32_t numViews, D3D12_VERTEX_BUFFER_VIEW* views) const;
		void SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW*) const;

		void SetRenderTargets(
			uint32_t count, D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, bool isSingleHandle, D3D12_CPU_DESCRIPTOR_HANDLE* dsv) const;
		void ClearRTV(CD3DX12_CPU_DESCRIPTOR_HANDLE const& rtv, glm::vec4 const& clearColor);
		void ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE const& dsv, float clearColor);

		void SetGraphicsRoot32BitConstants(
			uint32_t rootParamIdx, uint32_t count, void const* srcData, uint32_t dstOffset) const;

		void DrawIndexedInstanced(
			uint32_t numIndexes, uint32_t numInstances, uint32_t idxStartOffset, int32_t baseVertexOffset, uint32_t instanceOffset) const;

		void TransitionResource(ID3D12Resource* resources, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) const;

		D3D12_COMMAND_LIST_TYPE GetType() const;
		ID3D12GraphicsCommandList2* GetD3DCommandList() const;

		// TODO: Should we be providing access to this? Or, just handle it internally via our command list interface?
		dx12::GPUDescriptorHeap* GetGPUDescriptorHeap() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_commandList;
		D3D12_COMMAND_LIST_TYPE m_type;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
		uint64_t m_fenceValue; // When the command allocator can be reused


	private:
		std::unique_ptr<dx12::GPUDescriptorHeap> m_gpuDescriptorHeaps;


	private: // No copying allowed
		CommandList() = delete;
		CommandList(CommandList const&) = delete;
		CommandList(CommandList&&) = delete;
		CommandList& operator=(CommandList const&) = delete;
	};


	inline bool CommandList::GetFenceValue() const
	{
		return m_fenceValue;
	}


	inline void CommandList::SetFenceValue(uint64_t fenceValue)
	{
		m_fenceValue = fenceValue;
	}


	inline void CommandList::Reset(ID3D12PipelineState* pso) const
	{
		// Note: pso is optional; Sets a dummy PSO if nullptr. TODO: Accept a PSO?

		m_commandList->Reset(m_commandAllocator.Get(), pso);


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
		m_commandList->Close();
	}


	inline void CommandList::SetPipelineState(ID3D12PipelineState* pso) const
	{
		// TODO: Should we cache the dx12::PipelineState object on the command list?
		// We'd be able to use it to assert various things for sanity

		m_commandList->SetPipelineState(pso);
	}


	inline void CommandList::SetGraphicsRootSignature(dx12::RootSignature const& rootSig)
	{
		m_gpuDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		m_commandList->SetGraphicsRootSignature(rootSig.GetD3DRootSignature());
	}


	inline void CommandList::SetPrimitiveType(D3D_PRIMITIVE_TOPOLOGY topologyType) const
	{
		m_commandList->IASetPrimitiveTopology(topologyType);
	}


	inline void CommandList::SetVertexBuffers(uint32_t startSlot, uint32_t numViews, D3D12_VERTEX_BUFFER_VIEW* views) const
	{
		m_commandList->IASetVertexBuffers(startSlot, numViews, views);
	}


	inline void CommandList::SetIndexBuffer(D3D12_INDEX_BUFFER_VIEW* view) const
	{
		m_commandList->IASetIndexBuffer(view);
	}


	inline void CommandList::SetRenderTargets(
		uint32_t count, D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, bool isSingleHandle, D3D12_CPU_DESCRIPTOR_HANDLE* dsv) const
	{
		// NOTE: isSingleHandle == true specifies that the rtvs are contiguous in memory, thus count rtv descriptors
		// will be found by offsetting from rtvs[0]. Otherwise, it is assumed rtvs is an array of descriptor pointers

		m_commandList->OMSetRenderTargets(count, rtvs, isSingleHandle, dsv);
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
		uint32_t numIndexes, uint32_t numInstances, uint32_t idxStartOffset, int32_t baseVertexOffset, uint32_t instanceOffset) const
	{
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


	inline dx12::GPUDescriptorHeap* CommandList::GetGPUDescriptorHeap() const
	{
		return m_gpuDescriptorHeaps.get();
	}
}