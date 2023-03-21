// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "PipelineState_DX12.h"
#include "RootSignature_DX12.h"


namespace dx12
{
	// TODO: Figure out how we're handling sampler descriptors


	class GPUDescriptorHeap
	{
	public:
		// TODO: This should probably be dynamic: We might want to allocate a set number of descriptors for a specific
		// usage (E.g. ImGui)
		static constexpr uint32_t k_totalDescriptors = 512; // Total GPU-visible descriptors allocated
	
		static constexpr uint32_t k_totalRootSigDescriptorTableIndices = 32; // No. of root signature descriptor table indices
		static_assert(k_totalRootSigDescriptorTableIndices == dx12::RootSignature::k_totalRootSigDescriptorTableIndices);
		
		// TODO: According to the D3D specs, we're given a hard limit of 64 DWORDS of space. We should track the total
		// space used, instead of hard-limiting ourselves to 32 root signature indicies
		// static constexpr uint32_t k_maxRootArgDWORDS = 64; // Max root arg size = 64 DWORDS	


	public:
		enum InlineDescriptorType
		{
			CBV,
			SRV,
			UAV,

			//Sampler, // TODO: Handle samplers

			InlineRootType_Count
		};

	public:
		GPUDescriptorHeap(ID3D12GraphicsCommandList*, D3D12_COMMAND_LIST_TYPE, D3D12_DESCRIPTOR_HEAP_TYPE);
		~GPUDescriptorHeap();

		void ParseRootSignatureDescriptorTables(dx12::RootSignature const&);

		// Register a set of CPU descriptors for copy to a GPU-visible heap when CommitDescriptorTables() is called
		// Each descriptor table/range entry = 1 DWORD each
		void SetDescriptorTable(
			uint32_t rootParamIdx, const D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count);

		// Set resource views directly in the GPU-visible descriptor heap:
		void SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource*); // = 1 DWORD each
		void SetInlineSRV(uint32_t rootParamIdx, ID3D12Resource*); // = 2 DWORDS each
		void SetInlineUAV(uint32_t rootParamIdx, ID3D12Resource*); // = 2 DWORDS each

		// Copy all of our cached descriptors to our internal GPU-visible descriptor heap
		void Commit();

		ID3D12DescriptorHeap* GetD3DDescriptorHeap() const;


	private:
		void CommitDescriptorTables();
		void CommitInlineDescriptors();

		uint32_t GetNumDirtyTableDescriptors() const; // How many descriptors need to be (re)copied into the GPU-visible heap?

		
	private:
		ID3D12GraphicsCommandList* const m_owningCommandList;
		const D3D12_COMMAND_LIST_TYPE m_owningCommandListType;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		const size_t m_elementSize; 

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;


	private:
		// Copies of all of our CPU-visible descriptors that will be in our final GPU-visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE m_descriptorHandleCache[k_totalDescriptors];

		struct DescriptorTableCache // Details of the descriptor tables located within our final GPU-visible heap
		{
			D3D12_CPU_DESCRIPTOR_HANDLE* m_baseDescriptor;
			uint32_t m_numElements;
		};
		DescriptorTableCache m_descriptorTables[k_totalRootSigDescriptorTableIndices];

		// 1 bit per root signature index that contains a descriptor table
		uint32_t m_descriptorTableIdxBitmask; 

		// 1 bit per dirty descriptor table index. Marked when SetDescriptorTable() is called
		uint32_t m_dirtyDescriptorTableIdxBitmask; 

		// Inline root signature descriptors:
		D3D12_GPU_VIRTUAL_ADDRESS m_inlineDescriptors[InlineRootType_Count][k_totalRootSigDescriptorTableIndices];
		uint32_t m_dirtyInlineDescriptorIdxBitmask[InlineRootType_Count]; // Marked when SetInline_() is called
	};
}