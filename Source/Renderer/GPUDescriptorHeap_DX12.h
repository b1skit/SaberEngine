// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RootSignature_DX12.h"

#include <d3d12.h>
#include <wrl.h>

// TODO: Handle sampler descriptors


namespace dx12
{
	enum CommandListType : uint8_t; // CommandList_DX12.h
	class DescriptorAllocation;
	

	class GPUDescriptorHeap
	{
	public:
		// The total CPU-visible descriptors cached/Size of the GPU-visible descriptor heap
		static constexpr uint32_t k_totalDescriptors = 2048; // TODO: Should this be dynamic?
	
		static constexpr uint32_t k_totalRootSigEntries = 32; // No. of root signature indexes
		SEStaticAssert(k_totalRootSigEntries == dx12::RootSignature::k_totalRootSigEntries,
			"RootSignature and GPUDescriptorHeap are out of sync");
		
		// TODO: According to the D3D specs, we're given a hard limit of 64 DWORDS of space. We should track the total
		// space used, instead of hard-limiting ourselves to 32 root signature indicies
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits

	public:
		enum InlineDescriptorType : uint8_t
		{
			CBV,
			SRV,
			UAV,
			// Note: We do not maintain a Sampler descriptor heap

			InlineRootType_Count
		};

	public:		
		GPUDescriptorHeap(dx12::CommandListType, ID3D12GraphicsCommandList*);

		~GPUDescriptorHeap();

		ID3D12DescriptorHeap* GetD3DDescriptorHeap() const;

		void Reset();

		void ParseRootSignatureDescriptorTables(dx12::RootSignature const*);

		// Register a set of CPU descriptors for copy to a GPU-visible heap when CommitDescriptorTables() is called
		// Each descriptor/range entry = 1 DWORD each.
		// Note: offset & count can be used to set individual descriptors within a table located at a given rootParamIdx
		void SetDescriptorTableEntry(
			uint32_t rootParamIdx, D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count);

		// Set resource views directly in the GPU-visible descriptor heap:
		void SetInlineCBV(uint32_t rootParamIdx, ID3D12Resource*, uint64_t alignedByteOffset); // = 1 DWORD each
		void SetInlineSRV(uint32_t rootParamIdx, ID3D12Resource*, uint64_t alignedByteOffset); // = 2 DWORDS each
		void SetInlineUAV(uint32_t rootParamIdx, ID3D12Resource*, uint64_t alignedByteOffset); // = 2 DWORDS each

		void Commit(); // Copy all of our cached descriptors to our internal GPU-visible descriptor heap

		// Directly write descriptors to the GPU-visible descriptor heap/stack. Does not modify any metadata, other than
		// the GPU-visible descriptor CPU/GPU heap base offsets
		D3D12_GPU_DESCRIPTOR_HANDLE CommitToGPUVisibleHeap(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> const&);


	private:
		void CommitDescriptorTables();
		void CommitInlineDescriptors();

		uint32_t GetNumDirtyTableDescriptors() const; // How many descriptors need to be (re)copied into the GPU-visible heap?

		
	private:
		const CommandListType m_owningCommandListType;
		ID3D12GraphicsCommandList* const m_owningCommandList;
		
		const D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		const size_t m_elementSize; 


	private: // Descriptor tables:

		// Shader-visible descriptor heap. Used as a stack for storing descriptors held by descriptor tables
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_gpuDescriptorHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_gpuDescriptorHeapCPUBase; 
		D3D12_GPU_DESCRIPTOR_HANDLE m_gpuDescriptorHeapGPUBase;

		// CPU-visible descriptors (copies) that will be in our final GPU-visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE m_cpuDescriptorHeapCache[k_totalDescriptors];
		                
		// Details of the descriptor tables located within m_cpuDescriptorHeapCache (our CPU-visible descriptor cache)
		struct CPUDescriptorCacheMetadata 
		{
			D3D12_CPU_DESCRIPTOR_HANDLE* m_baseDescriptor;
			uint32_t m_numElements;
		};
		CPUDescriptorCacheMetadata m_cpuDescriptorHeapCacheLocations[k_totalRootSigEntries];

		// Bits map to root signature indexes that contain descriptor tables. Copied from root signature during parsing
		uint32_t m_rootSigDescriptorTableIdxBitmask; 

		// 1 bit per *dirty* descriptor table at a given root sig index. Marked when SetDescriptorTableEntry() is called
		uint32_t m_dirtyDescriptorTableIdxBitmask;
		SEStaticAssert(k_totalRootSigEntries == (sizeof(m_dirtyDescriptorTableIdxBitmask) * 8),
			"Not enough bits in the m_dirtyDescriptorTableIdxBitmask to represent all root signature entries");


	private: // Inline root descriptors:

		// 1 array entry each for CBVs, SRVs, UAVs, Samplers:
		D3D12_GPU_VIRTUAL_ADDRESS m_inlineDescriptors[InlineRootType_Count][k_totalRootSigEntries];

		uint32_t m_dirtyInlineDescriptorIdxBitmask[InlineRootType_Count]; // Marked during SetInlineCBV/SRV/UAV() calls
		SEStaticAssert(k_totalRootSigEntries == (sizeof(m_dirtyInlineDescriptorIdxBitmask[0]) * 8),
			"Not enough bits in the m_dirtyInlineDescriptorIdxBitmask to represent all root signature entries");

	private: // Debugging and null descriptor initialization
		void SetNullDescriptors(dx12::RootSignature const* rootSig);

		// Debug: Track inline descriptors seen while parsing the root sig, so we can assert *something* is set for them
		uint32_t m_unsetInlineDescriptors;

		dx12::RootSignature const* m_currentRootSig; // The most recently parsed root sig (for debugging purposes)
	};


	inline ID3D12DescriptorHeap* GPUDescriptorHeap::GetD3DDescriptorHeap() const
	{
		return m_gpuDescriptorHeap.Get();
	}
}