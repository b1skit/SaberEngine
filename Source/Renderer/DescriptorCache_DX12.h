// � 2024 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "TextureView.h"


namespace re
{
	class Buffer;
	class BufferView;
	class Texture;
}

namespace dx12
{
	class Context;


	class DescriptorCache
	{
	public:
		enum class DescriptorType : uint8_t
		{
			SRV,
			UAV,
			CBV,
			RTV,
			DSV,

			DescriptorType_Count
		};


	public:
		DescriptorCache(DescriptorType, dx12::Context*);
		~DescriptorCache();

		DescriptorCache(DescriptorCache&&) noexcept = default;
		DescriptorCache& operator=(DescriptorCache&&) noexcept = default;

		void Destroy();
		void EndFrame(uint64_t currentFrameNum);


	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(core::InvPtr<re::Texture> const&, re::TextureView const&);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Buffer const&, re::BufferView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Buffer const*, re::BufferView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(std::shared_ptr<re::Buffer const> const&, re::BufferView const&);


	private:
		inline void PerformPeriodicCleanup(uint64_t currentFrame);

		struct CacheEntry
		{
			util::HashKey m_hash;
			dx12::DescriptorAllocation m_allocation;
			uint64_t m_lastUsedFrame;

			CacheEntry(util::HashKey hash, dx12::DescriptorAllocation&& allocation, uint64_t currentFrame)
				: m_hash(hash), m_allocation(std::move(allocation)), m_lastUsedFrame(currentFrame) {}
		};

		struct CacheComparator
		{
			// Return true if 1st element is ordered before the 2nd
			inline bool operator()(dx12::DescriptorCache::CacheEntry const& cacheEntry, util::HashKey dataHash)
			{
				return cacheEntry.m_hash < dataHash;
			}

			inline bool operator()(dx12::DescriptorCache::CacheEntry const& a, dx12::DescriptorCache::CacheEntry const& b)
			{
				return a.m_hash < b.m_hash;
			}
		};

		std::mutex m_descriptorCacheMutex;
		std::vector<CacheEntry> m_descriptorCache;

		dx12::Context* m_context;
		ID3D12Device* m_deviceCache;

		DescriptorType m_descriptorType;

		// Cache cleanup parameters
		static constexpr uint64_t k_cacheCleanupFrameInterval = 300; // Clean up every 300 frames (~5 seconds at 60fps)
		static constexpr uint64_t k_maxUnusedFrames = 1800; // Remove descriptors unused for 1800 frames (~30 seconds at 60fps)
		uint64_t m_lastCleanupFrame;


	private: // No copying allowed:
		DescriptorCache(DescriptorCache const&) = delete;
		DescriptorCache& operator=(DescriptorCache const&) = delete;

		DescriptorCache() = delete;
	};
}