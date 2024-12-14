// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"
#include "Core/Util/HashUtils.h"
#include "Core/Util/MathUtils.h"
#include "Core/Util/ThreadProtector.h"

#include <d3d12.h>
#include <wrl.h>


namespace dx12
{
	class HeapManager;
	class HeapPage;
	struct HeapDesc;
	struct ResourceDesc;


	class HeapAllocation
	{
	public:
		HeapAllocation(); // Construct an invalid HeapAllocation
		HeapAllocation(HeapPage*, ID3D12Heap*, uint32_t baseOffset, uint32_t numBytes);

		~HeapAllocation();

		HeapAllocation(HeapAllocation&&) noexcept;
		HeapAllocation& operator=(HeapAllocation&&) noexcept;


	public:
		bool IsValid() const;

		void Free(); // Release and invalidate the HeapAllocation

		ID3D12Heap* GetHeap() const;
		uint32_t GetBaseOffset() const;
		uint32_t GetNumBytes() const; // Allocations are rounded up: Might be greater than the requested number of bytes


	private:
		HeapPage* m_owningHeapPage;
		ID3D12Heap* m_heap;
		uint32_t m_baseOffset;
		uint32_t m_numBytes;


	private: // No copies allowed:
		HeapAllocation(HeapAllocation const&) = delete;
		HeapAllocation& operator=(HeapAllocation const&) = delete;
	};


	// -----------------------------------------------------------------------------------------------------------------


	class HeapPage
	{
	public:
		HeapPage(HeapDesc const&, uint32_t pageSize);
		~HeapPage();

		HeapPage(HeapPage&&) noexcept = default;
		HeapPage& operator=(HeapPage&&) noexcept = default;


	protected:
		friend class PagedResourceHeap;
		HeapAllocation Allocate(uint32_t alignment, uint32_t numBytes);
		bool IsEmpty() const;

		friend class HeapAllocation;
		void Release(HeapAllocation const&);


	private:
		void Validate(); // _DEBUG only


	private:
		struct PageBlock
		{
			uint32_t m_baseOffset = 0;
			uint32_t m_numBytes = 0;

			PageBlock(uint32_t baseOffset, uint32_t numBytes);
			PageBlock(HeapAllocation const&);

			bool operator<(PageBlock const&) { return true; }

			bool CanFit(uint32_t alignment, uint32_t requestedNumBytes) const;
		};


	private:
		uint32_t m_pageSize; // Variable: The heap manager may request larger than default resources if required
		uint32_t m_minAlignmentSize; // 4KB for small textures, or 64KB for small MSAA textures
		uint32_t m_heapAlignment; // 64KB, or 4MB if the heap might contain MSAA textures

		Microsoft::WRL::ComPtr<ID3D12Heap> m_heap;

		std::list<PageBlock> m_freeBlocks;

		struct SizeOrderedPageBlockListItr
		{
			std::list<PageBlock>::iterator m_pageBlockItr;

			bool operator<(SizeOrderedPageBlockListItr const& rhs) const;
			bool operator<(PageBlock const& rhs) const;
		};
		std::set<SizeOrderedPageBlockListItr> m_freeBlocksBySize;

		std::mutex m_freeBlocksMutex; // For both m_freeBlocks and m_freeBlocksBySize

		util::ThreadProtector m_threadProtector;


	private: // No copies allowed:
		HeapPage() = delete;
		HeapPage(HeapPage const&) = delete;
		HeapPage& operator=(HeapPage const&) = delete;
	};


	// -----------------------------------------------------------------------------------------------------------------


	// Describes the type of heap that will back each page
	struct HeapDesc
	{
		HeapDesc(
			D3D12_HEAP_TYPE, 
			uint32_t alignment, 
			bool allowMSAATextures, 
			uint32_t creationNodeMask,
			uint32_t visibleNodeMask);

		D3D12_HEAP_TYPE m_heapType;
		D3D12_HEAP_FLAGS m_heapFlags;
		uint32_t m_alignment;
		uint32_t m_creationNodeMask = 0;
		uint32_t m_visibleNodeMask = 0;
		bool m_allowMSAATextures = false;
	};


	class PagedResourceHeap
	{
	public:
		PagedResourceHeap(HeapDesc const&);

		PagedResourceHeap(PagedResourceHeap&&) noexcept = default;
		PagedResourceHeap& operator=(PagedResourceHeap&&) noexcept = default;

		~PagedResourceHeap() = default;


	public:
		HeapAllocation GetAllocation(uint32_t numBytes);

		void EndOfFrame();


	private:
		HeapDesc m_heapDesc;

		static constexpr uint32_t k_defaultPageSize = 64 * 1024 * 1024; // 64MB
		uint32_t m_alignment;

		std::vector<std::unique_ptr<HeapPage>> m_pages;
		
		// Page access is all (currently) handled through the HeapManager; we use a thread protector in lieu of a mutex
		util::ThreadProtector m_threadProtector;

		std::unordered_map<HeapPage const*, uint8_t> m_emptyPageFrameCount;
		static constexpr uint8_t k_numEmptyFramesBeforePageRelease = 10; // No. consecutive empty frames before page release


	private: // No copies allowed:
		PagedResourceHeap(PagedResourceHeap const&) = delete;
		PagedResourceHeap& operator=(PagedResourceHeap const&) = delete;
	};


	// -----------------------------------------------------------------------------------------------------------------


	struct ResourceDesc
	{
		D3D12_RESOURCE_DESC m_resourceDesc;
		D3D12_CLEAR_VALUE m_optimizedClearValue;
		D3D12_HEAP_TYPE m_heapType;
		D3D12_RESOURCE_STATES m_initialState;
		bool m_isMSAATexture;
		bool m_createAsComitted;
	};


	class GPUResource
	{
	protected:
		friend class HeapManager;
		struct PrivateCTORToken {};


	public:
		GPUResource() = default; // Initialzes as invalid

		// Construct a GPUResource from an existing ID3D12Resource. 
		// Note: Lifetime is not managed by the heap manager (no deferred delete etc).
		// Useful for pre-existing/self-managed committed resources
		GPUResource(Microsoft::WRL::ComPtr<ID3D12Resource>, D3D12_RESOURCE_STATES initialState, wchar_t const* name);

		// Create a committed GPU resource.
		// Note: Lifetime is not managed by the heap manager (no deferred delete etc).
		GPUResource(HeapManager*, ResourceDesc const&, wchar_t const* name, PrivateCTORToken);

		// HeapManager managed GPUResource CTOR. Use HeapManager::CreateResource().
		GPUResource(HeapManager*, ResourceDesc const&, HeapAllocation&&, wchar_t const* name, PrivateCTORToken);

		GPUResource(GPUResource&&) noexcept;
		GPUResource& operator=(GPUResource&&) noexcept;

		~GPUResource();


	public:		
		HRESULT Map(uint32_t subresourceIdx, D3D12_RANGE const* readRange, void** dataOut);
		void Unmap(uint32_t subresourceIdx, D3D12_RANGE const* writtenRange);

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

		ID3D12Resource* Get();
		ID3D12Resource const* Get() const;

		bool IsValid() const;
		void Free(); // Release and invalidate the GPUResource

	protected:
		friend class HeapManager;
		void Invalidate();

	private:
		void SetName(wchar_t const*);


	private:
		HeapAllocation m_heapAllocation; // Note: Deferred deletion is managed by the HeapManager
		Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
		HeapManager* m_heapManager; // Note: Always be populated so GPUResources can all use the deferred delete queue


	private: // No copies allowed:
		GPUResource(GPUResource const&) = delete;
		GPUResource& operator=(GPUResource const&) = default;
	};


	class HeapManager
	{
	public:
		HeapManager();
		HeapManager(HeapManager&&) noexcept = default;
		HeapManager& operator=(HeapManager&&) noexcept = default;

		~HeapManager();

		void Destroy();


	public:
		void Initialize();

		void EndOfFrame(uint64_t frameNum);


	public:
		std::unique_ptr<GPUResource> CreateResource(ResourceDesc const& resourceDesc, wchar_t const* name);


	protected:
		friend class GPUResource;
		void Release(GPUResource&);


	private:
		std::unordered_map<util::DataHash, std::unique_ptr<PagedResourceHeap>> m_pagedHeaps;
		std::shared_mutex m_pagedHeapsMutex;

		std::queue<std::pair<uint64_t, GPUResource>> m_deferredGPUResourceDeletions;
		std::recursive_mutex m_deferredGPUResourceDeletionsMutex;

		ID3D12Device2* m_device;

		uint8_t m_numFramesInFlight;

		bool m_canMixResourceTypes;


	private: // No copies allowed:
		HeapManager(HeapManager const&) = delete;
		HeapManager& operator=(HeapManager const&) = default;
	};


	// -----------------------------------------------------------------------------------------------------------------
	// HEADER IMPLEMENTATIONS:
	// -----------------------------------------------------------------------------------------------------------------


	inline bool HeapAllocation::IsValid() const
	{
		SEAssert(m_numBytes > 0 ||
			(m_owningHeapPage == nullptr && m_heap == nullptr && m_baseOffset == 0),
			"Page block should be completely populated or zeroed out to signify validity/invalidity");

		return m_numBytes > 0;
	};


	inline ID3D12Heap* HeapAllocation::GetHeap() const
	{
		return m_heap;
	}


	inline uint32_t HeapAllocation::GetBaseOffset() const
	{
		return m_baseOffset;
	}


	inline uint32_t HeapAllocation::GetNumBytes() const
	{
		return m_numBytes;
	}


	// -----------------------------------------------------------------------------------------------------------------


	inline HeapPage::PageBlock::PageBlock(uint32_t baseOffset, uint32_t numBytes)
		: m_baseOffset(baseOffset)
		, m_numBytes(numBytes)
	{
	}


	inline HeapPage::PageBlock::PageBlock(HeapAllocation const& resourceAllocation)
		: m_baseOffset(resourceAllocation.GetBaseOffset())
		, m_numBytes(resourceAllocation.GetNumBytes())
	{
	}


	inline bool HeapPage::PageBlock::CanFit(uint32_t alignment, uint32_t requestedNumBytes) const
	{
		const uint32_t alignedBaseOffset = util::RoundUpToNearestMultiple(m_baseOffset, alignment);
		const uint32_t offsetBytes = alignedBaseOffset - m_baseOffset;
		
		return (m_numBytes >= offsetBytes) && 
			((m_numBytes - offsetBytes) >= requestedNumBytes);
	}


	inline bool HeapPage::SizeOrderedPageBlockListItr::operator<(SizeOrderedPageBlockListItr const& rhs) const
	{
		if (m_pageBlockItr->m_numBytes == rhs.m_pageBlockItr->m_numBytes)
		{
			return m_pageBlockItr->m_baseOffset < rhs.m_pageBlockItr->m_baseOffset;
		}
		return m_pageBlockItr->m_numBytes < rhs.m_pageBlockItr->m_numBytes;
	}


	inline bool HeapPage::SizeOrderedPageBlockListItr::operator<(PageBlock const& rhs) const
	{
		if (m_pageBlockItr->m_numBytes == rhs.m_numBytes)
		{
			return m_pageBlockItr->m_baseOffset < rhs.m_baseOffset;
		}
		return m_pageBlockItr->m_numBytes < rhs.m_numBytes;
	}


	// -----------------------------------------------------------------------------------------------------------------


	inline ID3D12Resource* GPUResource::Get()
	{
		return m_resource.Get();
	}


	inline HRESULT GPUResource::Map(uint32_t subresourceIdx, D3D12_RANGE const* readRange, void** dataOut)
	{
		return m_resource->Map(subresourceIdx, readRange, dataOut);
	}


	inline void GPUResource::Unmap(uint32_t subresourceIdx, D3D12_RANGE const* writtenRange)
	{
		m_resource->Unmap(subresourceIdx, writtenRange);
	}


	inline D3D12_GPU_VIRTUAL_ADDRESS GPUResource::GetGPUVirtualAddress() const
	{
		return m_resource->GetGPUVirtualAddress();
	}


	inline ID3D12Resource const* GPUResource::Get() const
	{
		return m_resource.Get();
	}


	inline bool GPUResource::IsValid() const
	{
		return m_heapManager != nullptr;
	}

	
	inline void GPUResource::Invalidate()
	{
		m_heapManager = nullptr; // Prevent recursive re-enqueing
	}
}