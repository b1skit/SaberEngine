// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <wrl.h>
#include <d3d12.h>


namespace dx12
{
	class AllocationPage;
	class DescriptorAllocation;

	class CPUDescriptorHeapManager
	{
	public:
		enum HeapType : uint8_t
		{
			CBV_SRV_UAV,
			// Note: We do not maintain a Sampler descriptor heap

			// These types cannot be used with D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:
			RTV,
			DSV,

			HeapType_Count
		};
		static constexpr D3D12_DESCRIPTOR_HEAP_TYPE TranslateHeapTypeToD3DHeapType(HeapType);


	public:
		static constexpr uint32_t k_numDescriptorsPerPage = 256;

	public:
		CPUDescriptorHeapManager(HeapType);
		CPUDescriptorHeapManager(CPUDescriptorHeapManager&&) noexcept;

		~CPUDescriptorHeapManager();
		void Destroy();

		DescriptorAllocation Allocate(uint32_t count);

		// Call this after waiting on the fence value to ensure the allocations are no longer in use. Any allocations 
		// freed with a fence value <= fenceVal will be released
		void ReleaseFreedAllocations(uint64_t fenceVal);


	private:
		AllocationPage* AllocateNewPage();


	private:
		const HeapType m_type;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_d3dType;
		const uint32_t m_elementSize;

		std::vector<std::unique_ptr<AllocationPage>> m_allocationPages;
		std::set<size_t> m_freePageIndexes; // Sorted indexes of non-full m_allocationPages
		std::mutex m_allocationPagesIndexesMutex;


	private:
		CPUDescriptorHeapManager(CPUDescriptorHeapManager const&) = delete;
		CPUDescriptorHeapManager& operator=(CPUDescriptorHeapManager&&) = delete;
		CPUDescriptorHeapManager& operator=(CPUDescriptorHeapManager const&) = delete;
	};


	/******************************************************************************************************************/


	class DescriptorAllocation
	{
	public:
		DescriptorAllocation(); // Returns an Invalid allocation
		DescriptorAllocation(
			D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor, uint32_t descriptorSize, uint32_t count, AllocationPage*);

		DescriptorAllocation(DescriptorAllocation&&) noexcept;
		DescriptorAllocation& operator=(DescriptorAllocation&&) noexcept;

		~DescriptorAllocation();
		void Free(size_t fenceVal);

		bool IsValid() const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetBaseDescriptor() const;
		uint32_t GetNumDescriptors() const;
		uint32_t GetDescriptorSize() const;
		

	private:
		void MarkInvalid();


	private:
		D3D12_CPU_DESCRIPTOR_HANDLE m_baseDescriptor;
		uint32_t m_numDescriptors;
		uint32_t m_descriptorSize;

		AllocationPage* m_allocationPage;


	private: // Move only:
		DescriptorAllocation(DescriptorAllocation const&) = delete;
		
		DescriptorAllocation& operator=(DescriptorAllocation const&) = delete;		
	};


	/******************************************************************************************************************/


	class AllocationPage
	{
	public:
		AllocationPage(
			CPUDescriptorHeapManager::HeapType type, uint32_t elementSize, uint32_t totalElements, uint32_t pageIdx);
		AllocationPage(AllocationPage&&) = default;
		AllocationPage& operator=(AllocationPage&&) = default;
		~AllocationPage();

		DescriptorAllocation Allocate(uint32_t descriptorCount);

		void Free(DescriptorAllocation const&, uint64_t fenceVal); // Called by DescriptorAllocation::Free

		void ReleaseFreedAllocations(uint64_t fenceVal);

		uint32_t GetNumFreeElements() const;

	private:
		bool CanAllocate(uint32_t descriptorCount) const;
		void FreeRange(size_t offset, uint32_t numDescriptors);


	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_baseDescriptor;
		const CPUDescriptorHeapManager::HeapType m_type;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_d3dType;
		const uint32_t m_descriptorElementSize;
		const uint32_t m_totalElements;

		uint32_t m_numFreeElements;

		mutable std::mutex m_pageMutex;


	private:
		struct AllocationBlock;

		typedef std::map<size_t, AllocationBlock> FreeOffsetToSize;
		FreeOffsetToSize m_freeOffsetsToSizes;

		typedef std::multimap<uint32_t, FreeOffsetToSize::iterator> SizeToFreeOffset;
		SizeToFreeOffset m_sizesToFreeOffsets;

		struct AllocationBlock
		{
			uint32_t m_numElements;
			SizeToFreeOffset::iterator m_sizeToFreeOffsetsLocation;
		};

		
	private:
		struct FreedAllocation
		{
			size_t m_offset;
			uint32_t m_numElements;
			uint64_t m_fenceVal;
		};
		std::queue<FreedAllocation> m_deferredDeletions;


	private: // No copying allowed
		AllocationPage() = delete;
		AllocationPage(AllocationPage const&) = delete;
		AllocationPage& operator=(AllocationPage const&) = delete;
	};
}