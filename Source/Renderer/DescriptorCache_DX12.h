// © 2024 Adam Badke. All rights reserved.
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


	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(core::InvPtr<re::Texture> const&, re::TextureView const&);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Buffer const&, re::BufferView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(re::Buffer const*, re::BufferView const&);
		D3D12_CPU_DESCRIPTOR_HANDLE GetCreateDescriptor(std::shared_ptr<re::Buffer const> const&, re::BufferView const&);


	private:
		using CacheEntry = std::pair<util::HashKey, dx12::DescriptorAllocation>;

		struct CacheComparator
		{
			// Return true if 1st element is ordered before the 2nd
			inline bool operator()(dx12::DescriptorCache::CacheEntry const& cacheEntry, util::HashKey dataHash)
			{
				return cacheEntry.first < dataHash;
			}

			inline bool operator()(dx12::DescriptorCache::CacheEntry const& a, dx12::DescriptorCache::CacheEntry const& b)
			{
				return a.first < b.first;
			}
		};

		std::mutex m_descriptorCacheMutex;
		std::vector<CacheEntry> m_descriptorCache;

		dx12::Context* m_context;
		ID3D12Device* m_deviceCache;

		DescriptorType m_descriptorType;


	private: // No copying allowed:
		DescriptorCache(DescriptorCache const&) = delete;
		DescriptorCache& operator=(DescriptorCache const&) = delete;

		DescriptorCache() = delete;
	};
}