// © 2025 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchHandle.h"
#include "BatchPool.h"
#include "EffectDB.h"

#include "Core/ProfilingMarkers.h"


namespace
{
	void ValidateVertexStreams(gr::StageBatchHandle::ResolvedVertexBuffers const& m_vertexBuffers)
	{
#if defined(_DEBUG)

		std::unordered_set<uint8_t> seenSlots;
		seenSlots.reserve(re::VertexStream::k_maxVertexStreams);

		SEAssert(m_vertexBuffers[0].first, "Must have at least 1 non-null vertex stream");

		bool seenNull = false;
		for (size_t i = 0; i < re::VertexStream::k_maxVertexStreams; ++i)
		{
			if (m_vertexBuffers[i].first == nullptr)
			{
				seenNull = true;
			}
			SEAssert(!seenNull || m_vertexBuffers[i].first == nullptr,
				"Found a non-null entry after a null. Vertex streams must be tightly packed");

			SEAssert(m_vertexBuffers[i].first == nullptr ||
				m_vertexBuffers[i].second != re::VertexBufferInput::k_invalidSlotIdx,
				"Invalid bind slot detected");

			SEAssert(m_vertexBuffers[i].first == nullptr ||
				i + 1 == re::VertexStream::k_maxVertexStreams ||
				m_vertexBuffers[i + 1].first == nullptr ||
				(m_vertexBuffers[i].first->m_view.m_streamView.m_type < m_vertexBuffers[i + 1].first->m_view.m_streamView.m_type) ||
				m_vertexBuffers[i].second + 1 == m_vertexBuffers[i + 1].second,
				"Vertex streams of the same type must be stored in monotoically-increasing slot order");

			if (m_vertexBuffers[i].first != nullptr)
			{
				SEAssert(!seenSlots.contains(m_vertexBuffers[i].second), "Duplicate slot index detected");
				seenSlots.emplace(m_vertexBuffers[i].second);
			}
		}

#endif
	}


	bool IsBatchAndShaderTopologyCompatible(
		gr::MeshPrimitive::PrimitiveTopology topologyMode, re::RasterizationState::PrimitiveTopologyType topologyType)
	{
		switch (topologyType)
		{
		case re::RasterizationState::PrimitiveTopologyType::Point:
		{
			return topologyMode == gr::MeshPrimitive::PrimitiveTopology::PointList;
		}
		break;
		case re::RasterizationState::PrimitiveTopologyType::Line:
		{
			return topologyMode == gr::MeshPrimitive::PrimitiveTopology::LineList ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::LineStrip ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::LineListAdjacency ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::LineStripAdjacency ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleList ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency;
		}
		break;
		case re::RasterizationState::PrimitiveTopologyType::Triangle:
		{
			return topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleList ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency;
		}
		break;
		case re::RasterizationState::PrimitiveTopologyType::Patch:
		{
			SEAssertF("Patch topology is (currently) unsupported");
		}
		break;
		default: SEAssertF("Invalid topology type");
		}
		return false;
	}
}

namespace gr
{
	BatchPool* BatchHandle::s_batchPool = nullptr;


	BatchHandle::BatchHandle(PoolIndex batchIndex, gr::RenderDataID renderDataID) noexcept
		: m_poolIndex(batchIndex)
		, m_renderDataID(renderDataID)
	{
		s_batchPool->AddBatchRef(m_poolIndex); // Increment the ref count for the batch

#if defined(BATCH_HANDLE_DEBUG)
		m_batch = m_poolIndex != k_invalidPoolIndex ? s_batchPool->GetBatch(m_poolIndex) : nullptr;
#endif
	}


	BatchHandle::BatchHandle(BatchHandle&& rhs) noexcept
		: m_poolIndex(k_invalidPoolIndex)
		, m_renderDataID(k_invalidRenderDataID)
	{
		*this = std::move(rhs);
	}


	BatchHandle& BatchHandle::operator=(BatchHandle&& rhs) noexcept
	{
		if (this != &rhs)
		{
			if (m_poolIndex != k_invalidPoolIndex)
			{
				s_batchPool->ReleaseBatch(m_poolIndex); // Release any current batch reference
			}

			// Move operation: We invalidate the moved-from handle so no need to update the reference count
			m_poolIndex = rhs.m_poolIndex;
			rhs.m_poolIndex = k_invalidPoolIndex;

			m_renderDataID = rhs.m_renderDataID;
			rhs.m_renderDataID = k_invalidRenderDataID;

#if defined(BATCH_HANDLE_DEBUG)
			m_batch = m_poolIndex != k_invalidPoolIndex ? s_batchPool->GetBatch(m_poolIndex) : nullptr;
#endif
		}
		return *this;
	}


	BatchHandle::BatchHandle(BatchHandle const& rhs) noexcept
		: m_poolIndex(k_invalidPoolIndex)
		, m_renderDataID(k_invalidRenderDataID)
	{
		*this = rhs;
	}


	BatchHandle& BatchHandle::operator=(BatchHandle const& rhs) noexcept
	{
		if (this != &rhs)
		{
			if (m_poolIndex != k_invalidPoolIndex)
			{
				s_batchPool->ReleaseBatch(m_poolIndex); // Release any current batch reference
			}

			m_poolIndex = rhs.m_poolIndex;
			m_renderDataID = rhs.m_renderDataID;

			s_batchPool->AddBatchRef(m_poolIndex); // Increment the ref count for the batch

#if defined(BATCH_HANDLE_DEBUG)
			m_batch = m_poolIndex != k_invalidPoolIndex ? s_batchPool->GetBatch(m_poolIndex) : nullptr;
#endif
		}
		return *this;
	}


	BatchHandle::~BatchHandle() noexcept
	{
		if (m_poolIndex != k_invalidPoolIndex)
		{
			s_batchPool->ReleaseBatch(m_poolIndex);
		}
	}


	re::Batch const* BatchHandle::operator->() const noexcept
	{
		return s_batchPool->GetBatch(m_poolIndex);
	}


	re::Batch const& BatchHandle::operator*() const noexcept
	{
		return *s_batchPool->GetBatch(m_poolIndex);
	}


	// ---


	std::pair<re::VertexBufferInput const*, uint8_t> const& StageBatchHandle::GetResolvedVertexBuffer(
		uint8_t slotIdx) const&
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");

		SEAssert((*m_batchHandle).GetType() == re::Batch::BatchType::Raster,
			"Trying to get a vertex stream from a non-raster batch type. This is unexpected");

		return m_resolvedVertexBuffers[slotIdx];
	}


	re::VertexBufferInput const& StageBatchHandle::GetIndexBuffer() const
	{
		SEAssert(m_isResolved, "StageBatchHandle has not been resolved");

		SEAssert((*m_batchHandle).GetType() == re::Batch::BatchType::Raster,
			"Trying to get an index stream from a non-raster batch type. This is unexpected");

		return (*m_batchHandle).GetRasterParams().m_indexBuffer;
	}


	void StageBatchHandle::Resolve(
		effect::drawstyle::Bitmask stageDrawstyleBits, uint32_t instanceCount, effect::EffectDB const& effectDB)
	{
		SEBeginCPUEvent("StageBatchHandle::Resolve");

		if (m_isResolved) // e.g. Batches resolved in a previous frame
		{
			SEAssert(m_instanceCount == instanceCount, "Batch already resolved with a different instance count");

			SEEndCPUEvent(); // "StageBatchHandle::Resolve"
			return;
		}
		
		m_instanceCount = instanceCount;
		m_isResolved = true;

		SEAssert((*m_batchHandle).GetDataHash() != 0,
			"Batch data hash has not been computed. The builder should have called this as the last step");

		// Some specialized batches (e.g. ray tracing) don't have an EffectID
		SEAssert((*m_batchHandle).GetEffectID() != 0 || 
			(*m_batchHandle).GetType() == re::Batch::BatchType::RayTracing,
			"Invalid EffectID");

		// Resolve the shader:
		const effect::drawstyle::Bitmask finalDrawstyle = (*m_batchHandle).GetDrawstyleBits() | stageDrawstyleBits;
		if ((*m_batchHandle).GetEffectID() != 0)
		{
			m_batchShader = effectDB.GetResolvedShader((*m_batchHandle).GetEffectID(), finalDrawstyle);
		}

		SEAssert((*m_batchHandle).GetType() != re::Batch::BatchType::Raster ||
			IsBatchAndShaderTopologyCompatible(
				(*m_batchHandle).GetRasterParams().m_primitiveTopology,
				m_batchShader->GetRasterizationState()->GetPrimitiveTopologyType()),
			"Raster topology mode is incompatible with shader pipeline state topology type");

		// Resolve vertex input slots now that we've decided which shader will be used:
		if ((*m_batchHandle).GetType() == re::Batch::BatchType::Raster)
		{
#if defined(_DEBUG)
			// Validate the resolved vertex buffers are unpopulated:
			for (auto& entry : m_resolvedVertexBuffers)
			{
				SEAssert(entry.first == nullptr && entry.second == re::VertexBufferInput::k_invalidSlotIdx,
					"Found uninitialized resolved vertex buffers");
			}
#endif

			// Get the vertex buffers from the batch, choosing the overrides if available:
			re::Batch::RasterParams const& rasterParams = (*m_batchHandle).GetRasterParams();

			std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams> const& vertexBuffers =
				rasterParams.m_vertexStreamOverrides ? 
					*rasterParams.m_vertexStreamOverrides :
					rasterParams.m_vertexBuffers;

			uint8_t numVertexStreams = 0;
			bool needsRepacking = false;
			for (uint8_t i = 0; i < re::VertexStream::k_maxVertexStreams; ++i)
			{
				// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
				if (vertexBuffers[i].GetStream() == nullptr)
				{
					break;
				}

				const re::VertexStream::Type curStreamType = vertexBuffers[i].m_view.m_streamView.m_type;

				// Find consecutive streams with the same type, and resolve the final vertex slot from the shader
				uint8_t semanticIdx = 0; // Start at 0 to ensure we process the current stream
				while (i + semanticIdx < re::VertexStream::k_maxVertexStreams &&
					vertexBuffers[i + semanticIdx].GetStream() &&
					vertexBuffers[i + semanticIdx].m_view.m_streamView.m_type == curStreamType)
				{
					const uint8_t vertexAttribSlot = m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					if (vertexAttribSlot != re::VertexStreamMap::k_invalidSlotIdx)
					{
						// Copy the stream:
						m_resolvedVertexBuffers[i + semanticIdx].first = &vertexBuffers[i + semanticIdx];

						// Update the bind slot:
						m_resolvedVertexBuffers[i + semanticIdx].second =
							m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					}
					else
					{
						m_resolvedVertexBuffers[i + semanticIdx].first = nullptr;
						m_resolvedVertexBuffers[i + semanticIdx].second = re::VertexBufferInput::k_invalidSlotIdx;
						needsRepacking = true;
					}
					++semanticIdx;
					++numVertexStreams;
				}
				if (semanticIdx > 1) // Skip ahead: We've already handled all consecutive streams of the same type
				{
					i = i + (semanticIdx - 1); // -1 b/c of the last ++semanticIdx;
				}
			}

			if (needsRepacking)
			{
				uint8_t numValidStreams = 0;
				for (uint8_t i = 0; i < numVertexStreams; ++i)
				{
					if (m_resolvedVertexBuffers[i].first == nullptr)
					{
						uint8_t nextValidIdx = i + 1;
						while (nextValidIdx < numVertexStreams &&
							m_resolvedVertexBuffers[nextValidIdx].first == nullptr)
						{
							++nextValidIdx;
						}
						if (nextValidIdx < numVertexStreams &&
							m_resolvedVertexBuffers[nextValidIdx].first != nullptr)
						{
							m_resolvedVertexBuffers[i] = m_resolvedVertexBuffers[nextValidIdx];
							m_resolvedVertexBuffers[nextValidIdx] = { nullptr, re::VertexBufferInput::k_invalidSlotIdx };
							++numValidStreams;
						}
						else if (nextValidIdx == numVertexStreams)
						{
							break; // Didn't find anything valid in the remaining elements, no point continuing
						}
					}
					else
					{
						++numValidStreams;
					}
				}
			}

			ValidateVertexStreams(m_resolvedVertexBuffers); // _DEBUG only
		}

		SEEndCPUEvent(); // "StageBatchHandle::Resolve"
	}
}