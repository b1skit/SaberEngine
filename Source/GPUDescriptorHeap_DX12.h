// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>
#include <wrl.h>

#include "RootSignature_DX12.h"

// TODO: Figure out how (if?) we're handling sampler descriptors


namespace dx12
{
	class DescriptorAllocation;


	class GPUDescriptorHeap
	{
	public:
		// The total CPU-visible descriptors cached/Size of the GPU-visible descriptor heap
		static constexpr uint32_t k_totalDescriptors = 512; // TODO: Should this be dynamic?
	
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
			// Note: We do not maintain a Sampler descriptor heap

			InlineRootType_Count
		};

	public:
		GPUDescriptorHeap(ID3D12GraphicsCommandList*, D3D12_COMMAND_LIST_TYPE, D3D12_DESCRIPTOR_HEAP_TYPE);
		~GPUDescriptorHeap();

		ID3D12DescriptorHeap* GetD3DDescriptorHeap() const;

		void Reset();

		void ParseRootSignatureDescriptorTables(dx12::RootSignature const*);

		// Register a set of CPU descriptors for copy to a GPU-visible heap when CommitDescriptorTables() is called
		// Each descriptor table/range entry = 1 DWORD each.
		// Note: offset & count can be used to set individual descriptors within a table located at a given rootParamIdx
		void SetDescriptorTable(
			uint32_t rootParamIdx, dx12::DescriptorAllocation const& src, uint32_t offset, uint32_t count);

		// Set resource views directly in the GPU-visible descriptor heap:
		void SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource*); // = 1 DWORD each
		void SetInlineSRV(uint32_t rootParamIdx, ID3D12Resource*); // = 2 DWORDS each
		void SetInlineUAV(uint32_t rootParamIdx, ID3D12Resource*); // = 2 DWORDS each

		void Commit(); // Copy all of our cached descriptors to our internal GPU-visible descriptor heap


	private:
		void CommitDescriptorTables();
		void CommitInlineDescriptors();

		uint32_t GetNumDirtyTableDescriptors() const; // How many descriptors need to be (re)copied into the GPU-visible heap?

		
	private:
		ID3D12GraphicsCommandList* const m_owningCommandList;
		const D3D12_COMMAND_LIST_TYPE m_owningCommandListType;

		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		const size_t m_elementSize; 


	private: // Descriptor tables:

		// Shader-visible descriptor table heap. Used as a stack for storing descriptors held by descriptor tables
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_gpuDescriptorTableHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_gpuDescriptorTableHeapCPUBase; 
		D3D12_GPU_DESCRIPTOR_HANDLE m_gpuDescriptorTableHeapGPUBase;

		// CPU-visible descriptors (copies) that will be in our final GPU-visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE m_cpuDescriptorTableHeapCache[k_totalDescriptors];
		                
		// Details of the descriptor tables located within our CPU-visible descriptor cache
		struct CPUDescriptorTableCacheMetadata 
		{
			D3D12_CPU_DESCRIPTOR_HANDLE* m_baseDescriptor;
			uint32_t m_numElements;
		};
		CPUDescriptorTableCacheMetadata m_cpuDescriptorTableCacheLocations[k_totalRootSigDescriptorTableIndices];

		// Bits map to root signature indexes that contain descriptor tables. Copied from root signature during parsing
		uint32_t m_rootSigDescriptorTableIdxBitmask; 

		// 1 bit per *dirty* descriptor table index. Marked when SetDescriptorTable() is called
		uint32_t m_dirtyDescriptorTableIdxBitmask;


	private: // Inline root descriptors:

		// 1 array entry each for CBVs, SRVs, UAVs, Samplers:
		D3D12_GPU_VIRTUAL_ADDRESS m_inlineDescriptors[InlineRootType_Count][k_totalRootSigDescriptorTableIndices];
		uint32_t m_dirtyInlineDescriptorIdxBitmask[InlineRootType_Count]; // Marked during SetInlineCBV/SRV/UAV() calls


	private: // Debugging and null descriptor table initialization
		void SetNullDescriptors(dx12::RootSignature const* rootSig);

		// Debug: Track inline descriptors seen while parsing the root sig, so we can assert *something* is set for them
		uint32_t m_unsetInlineDescriptors;
	};


	inline ID3D12DescriptorHeap* GPUDescriptorHeap::GetD3DDescriptorHeap() const
	{
		return m_gpuDescriptorTableHeap.Get();
	}
}