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
		SEAssert(m_indexedBuffers.empty() && m_LUTTypeToLUTMetadata.empty(),
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

		m_LUTTypeToLUTMetadata.clear();
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
			auto entries = m_IDToLUTMetadataEntries.equal_range(id);
			for (auto& itr = entries.first; itr != entries.second; ++itr)
			{
				itr->second.m_lutMetadata->m_LUTBufferInputs.erase(itr->second.m_lutHash);
				itr->second.m_lutMetadata->m_initialDataHashToLUTBufferInputsHash.erase(itr->second.m_intialDataHash);
			}
			m_IDToLUTMetadataEntries.erase(id);
		}

		// Update the indexed buffers:
		for (auto const& indexedBuffer : m_indexedBuffers)
		{
			const bool didReallocate = indexedBuffer->UpdateBuffer(m_renderData);

			if (didReallocate)
			{
				// If the Buffer was internally reallocated, we must clear all of our existing cached LUT BufferInputs.
				// This should happen relatively infrequently, so we use the LUTBuffer type_index associated with our
				// IIndexedBufferInternal* to find and reset our LUTMetadata (rather than doing extra book keeping to
				// keep them associated)
				for (auto& lutWritingBuffersItr : m_lutWritingBuffers)
				{
					auto lutWritingBuffers = m_lutWritingBuffers.equal_range(lutWritingBuffersItr.first);
					for (auto& bufferItr = lutWritingBuffers.first; bufferItr != lutWritingBuffers.second; ++bufferItr)
					{
						if (bufferItr->second == indexedBuffer.get())
						{
							std::type_index const& LUTBufferType = lutWritingBuffersItr.first;

							auto lutMetadataItr = m_LUTTypeToLUTMetadata.find(LUTBufferType);
							if (lutMetadataItr != m_LUTTypeToLUTMetadata.end())
							{
								lutMetadataItr->second.m_LUTBufferInputs.clear();
								lutMetadataItr->second.m_initialDataHashToLUTBufferInputsHash.clear();
								lutMetadataItr->second.m_firstFreeBaseIdx = 0;
							}
						}
					}
				}
			}
		}

		SEEndCPUEvent();
	}
}