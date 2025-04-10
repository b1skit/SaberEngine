// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RootSignature_DX12.h"

#include <d3d12.h>
#include <wrl.h>

// TODO: Handle sampler descriptors


namespace dx12
{
	class CommandList;
	

	class GPUDescriptorHeap
	{
	public:
		static constexpr uint32_t k_maxRootSigEntries = 64; // Maximum no. of root signature indices
		SEStaticAssert(k_maxRootSigEntries == dx12::RootSignature::k_maxRootSigEntries,
			"RootSignature and GPUDescriptorHeap are out of sync");
		// Note: dx12::RootSignature validates that a root signature never exceeds the maximum 64 DWORDs in size


	public:
		enum InlineDescriptorType : uint8_t
		{
			CBV,
			SRV,
			UAV,

			InlineDescriptorType_Count
		};
		SEStaticAssert(InlineDescriptorType_Count == dx12::RootSignature::DescriptorType::Type_Count,
			"GPUDescriptorHeap and root signature are out of sync");

	public:		
		GPUDescriptorHeap(uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE, std::wstring const& debugName);

		GPUDescriptorHeap(GPUDescriptorHeap&&) noexcept = default;
		GPUDescriptorHeap& operator=(GPUDescriptorHeap&&) noexcept = default;
		~GPUDescriptorHeap() = default;
		

	public:
		ID3D12DescriptorHeap* GetD3DDescriptorHeap() const;

		void Reset();

		void SetRootSignature(dx12::RootSignature const*);

		// Register a set of CPU descriptors for copy to a GPU-visible heap when CommitDescriptorTables() is called
		// Note: offset & count can be used to set individual descriptors within a table located at a given rootParamIdx
		void SetDescriptorTableEntry(
			uint8_t rootIdx, D3D12_CPU_DESCRIPTOR_HANDLE src, uint32_t offset, uint32_t count);

		// Set an entire descriptor table range from CPU-visible descriptors hosted in an externally-managed heap
		void SetDescriptorTableFromExternalHeap(
			uint8_t rootIdx, D3D12_CPU_DESCRIPTOR_HANDLE* srcBase, uint32_t count);

		// Set resource views directly in the GPU-visible descriptor heap:
		void SetInlineCBV(uint8_t rootIdx, D3D12_GPU_VIRTUAL_ADDRESS);
		void SetInlineSRV(uint8_t rootIdx, D3D12_GPU_VIRTUAL_ADDRESS);
		void SetInlineUAV(uint8_t rootIdx, D3D12_GPU_VIRTUAL_ADDRESS);

		// Copy staged descriptors from CPU to the GPU-visible descriptor heap
		// Note: The command list must have already called SetDescriptorHeaps using our GetD3DDescriptorHeap()
		void Commit(dx12::CommandList&); 

		// Directly write descriptors to the GPU-visible descriptor heap/stack. Does not modify any metadata, other than
		// the GPU-visible descriptor CPU/GPU heap base offsets
		D3D12_GPU_DESCRIPTOR_HANDLE CommitToGPUVisibleHeap(std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> const&);


	private:
		void CommitDescriptorTables(dx12::CommandList&);
		void CommitInlineDescriptors(dx12::CommandList&);

		
	private:
		ID3D12Device* m_deviceCache;
		uint32_t m_numDescriptors;
		D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
		size_t m_elementSize;


	private: // Descriptor tables:

		// Shader-visible descriptor heap. Used as a stack for storing descriptors held by descriptor tables
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_gpuDescriptorHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_gpuDescriptorHeapCPUBase; 
		D3D12_GPU_DESCRIPTOR_HANDLE m_gpuDescriptorHeapGPUBase;

		// CPU-visible descriptors (copies) that will be in our final GPU-visible heap
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_cpuDescriptorHeapCache;
		                
		// Details of the descriptor tables located within m_cpuDescriptorHeapCache (our CPU-visible descriptor cache)
		struct CPUDescriptorCacheMetadata 
		{
			D3D12_CPU_DESCRIPTOR_HANDLE* m_baseDescriptor;
			uint32_t m_numElements;
		};
		CPUDescriptorCacheMetadata m_cpuDescriptorHeapCacheLocations[k_maxRootSigEntries];

		// Bits map to root signature indexes that contain descriptor tables. Copied from root signature during parsing
		uint64_t m_rootSigDescriptorTableIdxBitmask;

		// 1 bit per *dirty* descriptor table at a given root sig index. Marked when SetDescriptorTableEntry() is called
		uint64_t m_dirtyDescriptorTableIdxBitmask;
		SEStaticAssert(k_maxRootSigEntries == (sizeof(m_dirtyDescriptorTableIdxBitmask) * 8),
			"Not enough bits in the m_dirtyDescriptorTableIdxBitmask to represent all root signature entries");


	private: // Inline root descriptors:

		// 1 array entry each for CBVs, SRVs, UAVs, Samplers:
		D3D12_GPU_VIRTUAL_ADDRESS m_inlineDescriptors[InlineDescriptorType_Count][k_maxRootSigEntries];

		uint64_t m_dirtyInlineDescriptorIdxBitmask[InlineDescriptorType_Count]; // Marked during SetInlineCBV/SRV/UAV() calls
		SEStaticAssert(k_maxRootSigEntries == (sizeof(m_dirtyInlineDescriptorIdxBitmask[0]) * 8),
			"Not enough bits in the m_dirtyInlineDescriptorIdxBitmask to represent all root signature entries");


	private: // Debugging and null descriptor initialization
		void SetNullDescriptors(dx12::RootSignature const* rootSig);

		// Debug: Track inline descriptors seen while parsing the root sig, so we can assert *something* is set for them
		uint64_t m_unsetInlineDescriptors;

		dx12::RootSignature const* m_currentRootSig; // The most recently parsed root sig (for debugging purposes)


	private: // No copies allowed:
		GPUDescriptorHeap(GPUDescriptorHeap const&) noexcept = delete;
		GPUDescriptorHeap& operator=(GPUDescriptorHeap const&) noexcept = delete;
	};


	inline ID3D12DescriptorHeap* GPUDescriptorHeap::GetD3DDescriptorHeap() const
	{
		return m_gpuDescriptorHeap.Get();
	}
}