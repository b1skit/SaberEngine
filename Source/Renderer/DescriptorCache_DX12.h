// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "CPUDescriptorHeapManager_DX12.h"
#include "TextureView.h"

#include "Core/Util/HashUtils.h"


namespace re
{
	class Texture;
}

namespace dx12
{
	class DescriptorCache
	{
	public:
		enum class DescriptorType : uint8_t
		{
			SRV,
			UAV,
			RTV,
			DSV,

			DescriptorType_Count
		};


	public:
		DescriptorCache(DescriptorType);
		~DescriptorCache();

		DescriptorCache(DescriptorCache&&) noexcept = default;
		DescriptorCache& operator=(DescriptorCache&&) noexcept = default;

		void Destroy();


	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Texture const&, re::TextureView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Texture const*, re::TextureView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(std::shared_ptr<re::Texture const> const&, re::TextureView const&);


	private:
		using CacheEntry = std::pair<DataHash, dx12::DescriptorAllocation>;

		struct CacheComparator
		{
			// Return true if 1st element is ordered before the 2nd
			inline bool operator()(dx12::DescriptorCache::CacheEntry const& cacheEntry, DataHash dataHash)
			{
				return cacheEntry.first < dataHash;
			}

			inline bool operator()(dx12::DescriptorCache::CacheEntry const& a, dx12::DescriptorCache::CacheEntry const& b)
			{
				return a.first < b.first;
			}
		};

		std::vector<CacheEntry> m_descriptorCache;
		std::mutex m_descriptorCacheMutex;

		DescriptorType m_descriptorType;


	private: // No copying allowed:
		DescriptorCache(DescriptorCache const&) = delete;
		DescriptorCache& operator=(DescriptorCache const&) = delete;

		DescriptorCache() = delete;
	};
}