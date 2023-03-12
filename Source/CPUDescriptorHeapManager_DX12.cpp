// © 2023 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "CPUDescriptorHeapManager_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "RenderManager.h"


namespace dx12
{
	CPUDescriptorHeapManager::CPUDescriptorHeapManager(D3D12_DESCRIPTOR_HEAP_TYPE type)
		: m_type(type)
		, m_elementSize(re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>()
			->m_device.GetD3DDisplayDevice()->GetDescriptorHandleIncrementSize(type))
	{
	}


	CPUDescriptorHeapManager::CPUDescriptorHeapManager(CPUDescriptorHeapManager&& rhs) noexcept
		: m_type(rhs.m_type)
		, m_elementSize(rhs.m_elementSize)
	{
		std::lock_guard<std::mutex> rhsAllocationLock(rhs.m_allocationPagesIndexesMutex);
		std::lock_guard<std::mutex> allocationLock(m_allocationPagesIndexesMutex);

		m_allocationPages = std::move(rhs.m_allocationPages);
		m_freePageIndexes = std::move(rhs.m_freePageIndexes);
	}


	CPUDescriptorHeapManager::~CPUDescriptorHeapManager()
	{
		Destroy();
	}



	void CPUDescriptorHeapManager::Destroy()
	{
		ReleaseFreedAllocations(0); // Internally locks m_allocationPagesIndexesMutex

		std::lock_guard<std::mutex> allocationLock(m_allocationPagesIndexesMutex);

		m_freePageIndexes.clear();
		m_allocationPages.clear();
	}



	DescriptorAllocation CPUDescriptorHeapManager::Allocate(uint32_t count)
	{
		SEAssert("Invalid number of allocations requested", count > 0 && count <= k_numDescriptorsPerPage);

		std::lock_guard<std::mutex> allocationLock(m_allocationPagesIndexesMutex);

		for (auto freePageIdxItr = m_freePageIndexes.begin(); freePageIdxItr != m_freePageIndexes.end(); freePageIdxItr++)
		{
			DescriptorAllocation newAllocation = m_allocationPages[*freePageIdxItr]->Allocate(count);

			if (m_allocationPages[*freePageIdxItr]->GetNumFreeElements() == 0)
			{
				m_freePageIndexes.erase(freePageIdxItr);
			}

			if (newAllocation.IsValid())
			{
				return newAllocation;
			}
		}

		// If we made it this far, no allocation was successfully made
		AllocationPage* newPage = AllocateNewPage(); // m_allocationPagesIndexesMutex is already held

		if (count == k_numDescriptorsPerPage)
		{
			// If our first allocation is going to exhaust the page, pre-remove it from our free page list
			const auto& freePageLocation = m_freePageIndexes.find(m_allocationPages.size() - 1);

			SEAssert("Expected newPage would be the last element in our vector", 
				newPage == m_allocationPages[*freePageLocation].get());

			m_freePageIndexes.erase(freePageLocation);
		}

		return newPage->Allocate(count);
	}


	void CPUDescriptorHeapManager::ReleaseFreedAllocations(uint64_t frameNum)
	{
		std::lock_guard<std::mutex> allocationLock(m_allocationPagesIndexesMutex);

		for (size_t i = 0; i < m_allocationPages.size(); i++)
		{
			m_allocationPages[i]->ReleaseFreedAllocations(frameNum);

			if (m_allocationPages[i]->GetNumFreeElements() > 0)
			{
				// std::set contains unique keys only; We can safely insert the same index multiple times
				m_freePageIndexes.insert(i);
			}
		}
	}


	AllocationPage* CPUDescriptorHeapManager::AllocateNewPage()
	{
		// Note: m_allocationPagesIndexesMutex has been locked already
		m_allocationPages.emplace_back(std::make_unique<AllocationPage>(m_type, m_elementSize, k_numDescriptorsPerPage));

		// The new page currently has 0 allocations, so we can safely add it to our free page index list
		m_freePageIndexes.insert(m_allocationPages.size() - 1);

		return m_allocationPages.back().get();
	}


	/******************************************************************************************************************/


	AllocationPage::AllocationPage(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t elementSize, uint32_t numElementsPerPage)
		: m_type(type)
		, m_descriptorElementSize(elementSize)
		, m_totalElements(numElementsPerPage)
		, m_numFreeElements(0) // Updated when we make our 1st FreeRange() call
	{
		std::lock_guard<std::mutex> pageLock(m_pageMutex);

		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		
		// Create our CPU-visible descriptor heap:
		D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
		heapDescriptor.Type = m_type;
		heapDescriptor.NumDescriptors = m_totalElements;
		heapDescriptor.NodeMask = 0; // We only support a single GPU

		// Note: CBV/SRV/UAV and sampler descriptors will NOT be shader visible with this flag:
		heapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr = ctxPlatParams->m_device.GetD3DDisplayDevice()->CreateDescriptorHeap(
			&heapDescriptor, 
			IID_PPV_ARGS(&m_descriptorHeap));
		CheckHResult(hr, "Failed to create CPU-visible descriptor heap");
		
		m_baseDescriptor = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

		// Initialize our tracking with a single block of all descriptors:
		FreeRange(0, m_totalElements);
	}



	AllocationPage::~AllocationPage()
	{
		std::lock_guard<std::mutex> pageLock(m_pageMutex);

		SEAssert("Destroying a page before allocations have been freed", m_numFreeElements == m_totalElements);

		m_descriptorHeap = nullptr;
		m_baseDescriptor = {0};
	}


	uint32_t AllocationPage::GetNumFreeElements() const
	{
		return m_numFreeElements;
	}


	bool AllocationPage::CanAllocate(uint32_t descriptorCount) const
	{
		// Check that there is at least 1 block with a size >= the requested count
		return m_sizesToFreeOffsets.lower_bound(descriptorCount) != m_sizesToFreeOffsets.end();
	}


	DescriptorAllocation AllocationPage::Allocate(uint32_t descriptorCount)
	{
		std::lock_guard<std::mutex> pageLock(m_pageMutex);

		if (descriptorCount > m_numFreeElements)
		{
			return DescriptorAllocation();
		}

		const auto smallestSuitableBlock = m_sizesToFreeOffsets.lower_bound(descriptorCount);
		if (smallestSuitableBlock == m_sizesToFreeOffsets.end())
		{
			return DescriptorAllocation();
		}
		
		// Extract our block metadata:
		const uint32_t blockSize = smallestSuitableBlock->first;
		
		const auto offsetLocation = smallestSuitableBlock->second;
		const size_t offsetIdx = offsetLocation->first;

		SEAssert("Tracking tables are out of sync", 
			(offsetLocation != m_freeOffsetsToSizes.end() && smallestSuitableBlock != m_sizesToFreeOffsets.end()) ||
			(offsetLocation == m_freeOffsetsToSizes.end() && smallestSuitableBlock == m_sizesToFreeOffsets.end()));

		// Delete our existing entries:
		m_freeOffsetsToSizes.erase(offsetLocation);
		m_sizesToFreeOffsets.erase(smallestSuitableBlock);

		m_numFreeElements -= descriptorCount;

		// Compute our updated metadata, and Free any remaining allocations for reuse:
		const uint32_t remainingBlockSize = blockSize - descriptorCount;
		if (remainingBlockSize > 0)
		{
			const size_t newOffset = offsetIdx + descriptorCount;
			m_numFreeElements -= remainingBlockSize; // FreeRange will re-add the number of freed blocks to the count
			FreeRange(newOffset, remainingBlockSize);
		}

		return DescriptorAllocation(
			D3D12_CPU_DESCRIPTOR_HANDLE{ m_baseDescriptor.ptr + (m_descriptorElementSize * offsetIdx) }, 
			m_descriptorElementSize, 
			descriptorCount, 
			this);
	}


	void AllocationPage::Free(DescriptorAllocation const& allocation, size_t frameNum)
	{
		std::lock_guard<std::mutex> pageLock(m_pageMutex);

		const size_t offset =
			(allocation.GetFirstDescriptor().ptr - m_baseDescriptor.ptr) / m_descriptorElementSize;

		m_deferredDeletions.emplace(FreedAllocation{ offset, allocation.GetNumDescriptors(), frameNum});

		// Note: The DescriptorAllocation will mark itself invalid after returning from this function
	}


	void AllocationPage::ReleaseFreedAllocations(uint64_t frameNum)
	{
		std::lock_guard<std::mutex> pageLock(m_pageMutex);

		// Process the deferred deletion queue:
		while (!m_deferredDeletions.empty() && m_deferredDeletions.front().m_frameNum <= frameNum)
		{
			FreeRange(m_deferredDeletions.front().m_offset, m_deferredDeletions.front().m_numElements);

			m_deferredDeletions.pop();
		}
	}


	void AllocationPage::FreeRange(size_t offset, uint32_t numDescriptors)
	{
		// Note: m_pageMutex should already be locked

		// Add an entry to the offsets->sizes table (using a temporary dummy location in the sizes list):
		auto offsetLocation = m_freeOffsetsToSizes.emplace(
			offset, 
			AllocationBlock{ numDescriptors, m_sizesToFreeOffsets.end() });
		SEAssert("Failed to insert to the offset->size entry", offsetLocation.second == true);

		// Add an entry to the sizes->offsets table:
		auto sizeLocation = m_sizesToFreeOffsets.emplace(numDescriptors, offsetLocation.first);

		// Update the dummy sizes->offset location in our offsets->sizes table
		offsetLocation.first->second.m_sizeToFreeOffsetsLocation = sizeLocation;

		// Finally, update our count of the number of free elements
		m_numFreeElements += numDescriptors;

		// Merge our new block with its immediate left/right neighbors:
		auto MergeBlocks = [this](
			AllocationPage::FreeOffsetToSize::iterator& prevLocation,
			size_t& offset,
			AllocationPage::FreeOffsetToSize::iterator& offsetLocation)
		{
			const size_t prevOffset = prevLocation->first;
			AllocationBlock const& prevAllocation = prevLocation->second;

			if (prevOffset + prevAllocation.m_numElements == offset)
			{
				const size_t mergedOffset = prevOffset;
				const uint32_t mergedNumElements =
					prevAllocation.m_numElements + offsetLocation->second.m_numElements;

				// Remove the deprecated entries:
				// Note: std::map::emplace, std::map::erase, std::multimap::emplace, std::multimap::erase do not affect
				// existing references/iterators (except those that are erased)
				m_sizesToFreeOffsets.erase(offsetLocation->second.m_sizeToFreeOffsetsLocation);
				m_sizesToFreeOffsets.erase(prevLocation->second.m_sizeToFreeOffsetsLocation);
				m_freeOffsetsToSizes.erase(offsetLocation); // offsetLocation is now invalidated
				m_freeOffsetsToSizes.erase(prevLocation); // prevLocation is now invalidated

				// Insert our new combined entry into the free offsets list:
				AllocationBlock mergedAllocation = { mergedNumElements,	m_sizesToFreeOffsets.end() };
				auto mergedLocation = m_freeOffsetsToSizes.emplace(
					mergedOffset,
					std::move(mergedAllocation));
				SEAssert("Failed to insert to the offset->size entry", mergedLocation.second);

				// Insert our combined entry into the sizes->offsets table:
				auto mergedSizeLocation = m_sizesToFreeOffsets.emplace(mergedNumElements, mergedLocation.first);

				// Update the dummy sizes->offset location in our offsets->sizes table
				mergedLocation.first->second.m_sizeToFreeOffsetsLocation = mergedSizeLocation;

				// Finally, update our offset metadata for subsequent calls:
				offsetLocation = mergedLocation.first;
				offset = prevOffset;
			}
		};

		if (offsetLocation.first != m_freeOffsetsToSizes.begin())
		{
			auto prevLocation = std::prev(offsetLocation.first);
			MergeBlocks(prevLocation, offset, offsetLocation.first);
		}

		SEAssert("Invalid iterator", offsetLocation.first != m_freeOffsetsToSizes.end());

		auto nextLocation = std::next(offsetLocation.first);
		if (nextLocation != m_freeOffsetsToSizes.end())
		{
			MergeBlocks(offsetLocation.first, offset, nextLocation);
		}
	}


	/******************************************************************************************************************/


	DescriptorAllocation::DescriptorAllocation()
		: m_baseDescriptor{0}
		, m_descriptorSize(0)
		, m_numDescriptors(0)
		, m_allocationPage(nullptr)
	{
	}


	DescriptorAllocation::DescriptorAllocation(
		D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor, uint32_t descriptorSize, uint32_t count, AllocationPage* allocationPage)
		: m_baseDescriptor(baseDescriptor)
		, m_numDescriptors(count)
		, m_descriptorSize(descriptorSize)		
		, m_allocationPage(allocationPage)
	{
	}


	DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& rhs) noexcept
		: m_baseDescriptor(std::move(rhs.m_baseDescriptor))
		, m_numDescriptors(rhs.m_numDescriptors)
		, m_descriptorSize(rhs.m_descriptorSize)
		, m_allocationPage(std::move(rhs.m_allocationPage))
	{
		rhs.m_allocationPage = nullptr;
		rhs.m_baseDescriptor = {0};
	}


	DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& rhs) noexcept
	{
		if (&rhs != this)
		{
			m_baseDescriptor = std::move(rhs.m_baseDescriptor);
			m_numDescriptors = rhs.m_numDescriptors;
			m_descriptorSize = rhs.m_descriptorSize;
			m_allocationPage = rhs.m_allocationPage;
			rhs.MarkInvalid();
		}
		return *this;
	}


	DescriptorAllocation::~DescriptorAllocation()
	{
		Free(0);

		SEAssert("DescriptorAllocation has not been correctly invalidated", 
			m_baseDescriptor.ptr == 0 && m_allocationPage == nullptr);
	}


	void DescriptorAllocation::Free(size_t frameNum)
	{
		if (IsValid())
		{
			m_allocationPage->Free(*this, frameNum);
			MarkInvalid();
		}
	}


	bool DescriptorAllocation::IsValid() const
	{
		return m_baseDescriptor.ptr != 0;
	}


	void DescriptorAllocation::MarkInvalid()
	{
		m_baseDescriptor = { 0 };
		m_allocationPage = nullptr;
	}


	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetFirstDescriptor() const
	{
		return m_baseDescriptor;
	}


	uint32_t DescriptorAllocation::GetDescriptorSize() const
	{
		return m_descriptorSize;
	}


	uint32_t DescriptorAllocation::GetNumDescriptors() const
	{
		return m_numDescriptors;
	}
}