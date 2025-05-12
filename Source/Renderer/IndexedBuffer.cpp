// © 2025 Adam Badke. All rights reserved.
#include "IndexedBuffer.h"
#include "RenderObjectIDs.h"

#include "Core/Config.h"
#include "Core/ThreadPool.h"


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
				auto bufferInputItr = itr->second.m_lutMetadata->m_LUTBufferInputs.find(itr->second.m_lutHash);
				if (bufferInputItr != itr->second.m_lutMetadata->m_LUTBufferInputs.end())
				{
					itr->second.m_lutMetadata->Free(
						bufferInputItr->second.first.GetView().m_bufferView.m_firstElement,
						bufferInputItr->second.first.GetView().m_bufferView.m_numElements);

					itr->second.m_lutMetadata->m_LUTBufferInputs.erase(itr->second.m_lutHash);
				}
			}
			m_IDToLUTMetadataEntries.erase(id);
		}

		static const bool singleThreadIndexedBufferUpdates =
			core::Config::Get()->KeyExists(core::configkeys::k_singleThreadIndexedBufferUpdates) == true;

		// Update the indexed buffers:
		std::vector<std::future<bool>> bufferUpdateFutures;
		bufferUpdateFutures.reserve(m_indexedBuffers.size());

		for (size_t i = 0; i < m_indexedBuffers.size(); ++i)
		{
			IIndexedBufferInternal* indexedBufferPtr = m_indexedBuffers[i].get();
			gr::RenderDataManager const& renderData = m_renderData;

			if (singleThreadIndexedBufferUpdates == false)
			{
				bufferUpdateFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
					[indexedBufferPtr, &renderData]() -> bool
					{
						return indexedBufferPtr->UpdateBuffer(renderData);
					}));
			}
			else // Single threaded: Record an already-resolved future:
			{
				const bool didReallocate = indexedBufferPtr->UpdateBuffer(renderData);

				std::promise<bool> result;
				result.set_value(didReallocate);
				bufferUpdateFutures.emplace_back(result.get_future());
			}
		}
		

		for (size_t i = 0; i < m_indexedBuffers.size(); ++i)
		{
			// Wait for the update to be complete
			const bool didReallocate = bufferUpdateFutures[i].get();

			// Handle the result on the main thread:
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
						if (bufferItr->second == m_indexedBuffers[i].get())
						{
							std::type_index const& LUTBufferType = lutWritingBuffersItr.first;

							auto lutMetadataItr = m_LUTTypeToLUTMetadata.find(LUTBufferType);
							if (lutMetadataItr != m_LUTTypeToLUTMetadata.end())
							{
								lutMetadataItr->second.Reset(); // Reset the LUT block allocation tracking
							}
						}
					}
				}
			}
		}

		SEEndCPUEvent();
	}
}