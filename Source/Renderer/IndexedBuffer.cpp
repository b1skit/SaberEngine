// � 2025 Adam Badke. All rights reserved.
#include "IndexedBuffer.h"

#include "Core/Config.h"
#include "Core/ThreadPool.h"


namespace gr
{
	IndexedBufferManager::IndexedBufferManager(gr::RenderDataManager const& renderData)
		: m_renderData(renderData)
		, m_ibmThreadProtector(false)
	{
	}


	IndexedBufferManager::~IndexedBufferManager()
	{
		SEAssert(m_indexedBuffers.empty() && m_LUTTypeToLUTMetadata.empty(),
			"IndexedBufferManager dtor called before Destroy()");
	}


	void IndexedBufferManager::Destroy()
	{
		util::ScopedThreadProtector lock(m_ibmThreadProtector);

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

		util::ScopedThreadProtector lock(m_ibmThreadProtector);

		static const bool singleThreadIndexedBufferUpdates =
			core::Config::Get()->KeyExists(core::configkeys::k_singleThreadIndexedBufferUpdates) == true;

		// Update the indexed buffers:
		std::vector<std::future<void>> bufferUpdateFutures;
		bufferUpdateFutures.reserve(m_indexedBuffers.size());

		for (size_t i = 0; i < m_indexedBuffers.size(); ++i)
		{
			IIndexedBufferInternal* indexedBufferPtr = m_indexedBuffers[i].get();
			gr::RenderDataManager const& renderData = m_renderData;

			if (singleThreadIndexedBufferUpdates == false)
			{
				bufferUpdateFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
					[indexedBufferPtr, &renderData]()
					{
						indexedBufferPtr->UpdateBuffer(renderData);
					}));
			}
			else // Single threaded: Record an already-resolved future:
			{
				indexedBufferPtr->UpdateBuffer(renderData);

				std::promise<void> result;
				bufferUpdateFutures.emplace_back(result.get_future());
			}
		}

		// Update LUT metadata:
		for (auto& entry : m_LUTTypeToLUTMetadata)
		{
			entry.second.Update();
		}

		// Wait for the updates to complete and handle any exceptions
		for (size_t i = 0; i < m_indexedBuffers.size(); ++i)
		{
			bufferUpdateFutures[i].get(); // This will rethrow any exceptions from the job
		}

		SEEndCPUEvent();
	}


	void IndexedBufferManager::ShowImGuiWindow() const
	{
		ImGui::Text(std::format("Managing {} indexed buffers:", m_indexedBuffers.size()).c_str());

		for (auto const& indexedBuffer : m_indexedBuffers)
		{
			indexedBuffer->ShowImGuiWindow();
		}
	}
}