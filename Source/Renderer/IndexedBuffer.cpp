// © 2025 Adam Badke. All rights reserved.
#include "IndexedBuffer.h"
#include "RenderObjectIDs.h"


namespace gr
{
	IndexedBufferManager::IndexedBufferManager(gr::RenderDataManager const& renderData)
		: m_renderData(renderData)
		, m_threadProtector(false)
	{
	}


	IndexedBufferManager::~IndexedBufferManager()
	{
		SEAssert(m_indexedBuffers.empty() && m_LUTBuffers.empty(),
			"IndexedBufferManager dtor called before Destroy()");
	}


	void IndexedBufferManager::Destroy()
	{
		util::ScopedThreadProtector lock(m_threadProtector);

		for (auto& indexedBuffer : m_indexedBuffers)
		{
			indexedBuffer->Destroy();
		}
		m_indexedBuffers.clear();

		m_LUTBuffers.clear();
	}


	void IndexedBufferManager::Update()
	{
		SEBeginCPUEvent("IndexedBufferManager::Update");

		util::ScopedThreadProtector lock(m_threadProtector);

		// If any render data associated with a RenderDataID has been deleted, destroy all of the associated BufferViews.
		// This is slight overkill, as a BufferView might not have been affected, but it allows for significantly less
		// book keeping at the cost of recreating otherwise unaffected BufferViews.
		// Note: Transform BufferViews are created via RenderDataIDs, thus no need to handle them as a special case here
		for (gr::RenderDataID id : m_renderData.GetIDsWithAnyDeletedData())
		{
			auto entries = m_IDToBufferInputs.equal_range(id);
			for (auto& itr = entries.first; itr != entries.second; ++itr)
			{
				LUTMetadata* lutMetadata = itr->second.first;
				const util::HashKey hashKey = itr->second.second;

				lutMetadata->m_LUTBufferInputs.erase(hashKey);
			}
			m_IDToBufferInputs.erase(id);
		}

		// Update the indexed buffers:
		for (auto const& indexedBuffer : m_indexedBuffers)
		{
			indexedBuffer->UpdateBuffer(m_renderData);
		}

		SEEndCPUEvent();
	}
}