// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "RenderManager.h"
#include "Shader.h"
#include "Texture.h"
#include "TextureView.h"
#include "VertexStream.h"

#include "Core/Assert.h"


namespace
{
	void ValidateBufferLifetimeCompatibility(re::Lifetime batchLifetime, re::BufferInput const& bufferInput)
	{
#if defined(_DEBUG)
		SEAssert(batchLifetime == bufferInput.GetLifetime() ||
			(batchLifetime == re::Lifetime::SingleFrame && bufferInput.GetLifetime() == re::Lifetime::Permanent),
			"Trying to set a buffer with a mismatching lifetime. Permanent batches cannot (currently) hold "
			"single frame buffers, as they'd incorrectly maintain their life beyond the frame. Single frame "
			"batches can hold any type of buffers (but should not be responsible for the lifetime of a "
			"permanent buffer as they're expensive to create/destroy)");
#endif
	}


	void ValidateVertexStreamLifetime(re::Lifetime batchLifetime, re::Lifetime vertexStreamLifetime)
	{
#if defined(_DEBUG)
		SEAssert(batchLifetime == re::Lifetime::SingleFrame ||
				(batchLifetime == re::Lifetime::Permanent &&
					vertexStreamLifetime == re::Lifetime::Permanent),
			"Cannot add a vertex stream with a single frame lifetime to a permanent batch");
#endif
	}


	void ValidateVertexStreams(
		re::Lifetime batchLifetime, 
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const& vertexBuffers)
	{
#if defined(_DEBUG)

		std::unordered_set<uint8_t> seenSlots;
		seenSlots.reserve(gr::VertexStream::k_maxVertexStreams);

		SEAssert(vertexBuffers[0].GetStream(), "Must have at least 1 non-null vertex stream");
		
		bool seenNull = false;
		for (size_t i = 0; i < gr::VertexStream::k_maxVertexStreams; ++i)
		{
			if (vertexBuffers[i].GetStream() == nullptr)
			{
				seenNull = true;
			}
			SEAssert(!seenNull || vertexBuffers[i].GetStream() == nullptr,
				"Found a non-null entry after a null. Vertex streams must be tightly packed");

			if (vertexBuffers[i].GetStream() && vertexBuffers[i].GetBuffer()) // Buffer might not have been created yet
			{
				ValidateVertexStreamLifetime(batchLifetime, vertexBuffers[i].GetBuffer()->GetLifetime());
			}

			SEAssert(vertexBuffers[i].GetStream() == nullptr ||
				vertexBuffers[i].m_bindSlot != re::VertexBufferInput::k_invalidSlotIdx,
				"Invalid bind slot detected");
			
			SEAssert(vertexBuffers[i].GetStream() == nullptr ||
				i + 1 == gr::VertexStream::k_maxVertexStreams ||
				vertexBuffers[i + 1].GetStream() == nullptr ||
				(vertexBuffers[i].m_view.m_streamView.m_type < vertexBuffers[i + 1].m_view.m_streamView.m_type) ||
				vertexBuffers[i].m_bindSlot + 1 == vertexBuffers[i + 1].m_bindSlot,
				"Vertex streams of the same type must be stored in monotoically-increasing slot order");

			if (vertexBuffers[i].GetStream() != nullptr)
			{
				SEAssert(!seenSlots.contains(vertexBuffers[i].m_bindSlot), "Duplicate slot index detected");
				seenSlots.emplace(vertexBuffers[i].m_bindSlot);
			}
		}

#endif
	}

	void ValidateVertexStreamOverrides(
		re::Lifetime batchLifetime,
		std::array<core::InvPtr<gr::VertexStream>, gr::VertexStream::k_maxVertexStreams> const& streams,
		re::Batch::VertexStreamOverride const* overrides)
	{
#if defined(_DEBUG)
		if (!overrides)
		{
			return;
		}

		for (size_t i = 0; i < gr::VertexStream::k_maxVertexStreams; ++i)
		{
			SEAssert((streams[i] == nullptr) == ((*overrides)[i].GetStream() == nullptr),
				"Vertex stream overrides must map 1:1 with mesh primitive buffers");

			if ((*overrides)[i].GetStream())
			{
				ValidateVertexStreamLifetime(batchLifetime, (*overrides)[i].GetBuffer()->GetLifetime());

				SEAssert(i + 1 == gr::VertexStream::k_maxVertexStreams ||
					(*overrides)[i + 1].GetStream() == nullptr ||
					((*overrides)[i].m_view.m_streamView.m_type < (*overrides)[i + 1].m_view.m_streamView.m_type) ||
					(*overrides)[i].m_bindSlot + 1 == (*overrides)[i + 1].m_bindSlot ||
					((*overrides)[i].m_bindSlot == re::VertexBufferInput::k_invalidSlotIdx && 
						(*overrides)[i + 1].m_bindSlot == re::VertexBufferInput::k_invalidSlotIdx),
					"Vertex streams of the same type must be stored in monotoically-increasing slot order");
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

namespace re
{
	Batch::Batch(BatchType batchType)
		: m_lifetime(re::Lifetime::SingleFrame)
		, m_type(batchType)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
		, m_renderDataID(gr::k_invalidRenderDataID)
	{
		switch (m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));

			m_rasterParams = {};
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams = {};
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));

			m_rayTracingParams = {};
		}
		break;
		default: SEAssertF("Invalid type");
		}
	};


	Batch::Batch(Batch&& rhs) noexcept
	{
		switch (rhs.m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));
		}
		break;
		case BatchType::Compute:
		{
			// Zero-initialize for consistency
			memset(&m_computeParams, 0, sizeof(m_computeParams));
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));
		}
		break;
		default: SEAssertF("Invalid type");
		}

		*this = std::move(rhs);
	};


	Batch& Batch::operator=(Batch&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_lifetime = rhs.m_lifetime;
			m_type = rhs.m_type;

			switch (m_type)
			{
			case BatchType::Raster:
			{
				m_rasterParams = std::move(rhs.m_rasterParams);
			}
			break;
			case BatchType::Compute:
			{
				m_computeParams = std::move(rhs.m_computeParams);
			}
			break;
			case BatchType::RayTracing:
			{
				m_rayTracingParams = std::move(rhs.m_rayTracingParams);
			}
			break;
			default: SEAssertF("Invalid type");
			}

			m_batchShader = std::move(rhs.m_batchShader);
			m_effectID = std::move(rhs.m_effectID);

			m_drawStyleBitmask = rhs.m_drawStyleBitmask;
			rhs.m_drawStyleBitmask = 0;

			m_batchFilterBitmask = rhs.m_batchFilterBitmask;
			rhs.m_batchFilterBitmask = 0;

			m_batchBuffers = std::move(rhs.m_batchBuffers);

			m_batchTextureSamplerInputs = std::move(rhs.m_batchTextureSamplerInputs);
			m_batchRWTextureInputs = std::move(rhs.m_batchRWTextureInputs);

			m_batchRootConstants = std::move(rhs.m_batchRootConstants);

			SetDataHash(rhs.GetDataHash());
			rhs.ResetDataHash();
		}
		return *this;
	};


	Batch::Batch(Batch const& rhs) noexcept
	{
		switch (rhs.m_type)
		{
		case BatchType::Raster:
		{
			// We must zero-initialize our InvPtrs to ensure they don't contain garbage before initializing RasterParams
			memset(&m_rasterParams.m_vertexBuffers, 0, sizeof(m_rasterParams.m_vertexBuffers));
			memset(&m_rasterParams.m_indexBuffer, 0, sizeof(m_rasterParams.m_indexBuffer));
		}
		break;
		case BatchType::Compute:
		{
			// Zero-initialize for consistency
			memset(&m_computeParams, 0, sizeof(m_computeParams));
		}
		break;
		case BatchType::RayTracing:
		{
			// Zero-initialize to ensure shared_ptr doesn't contain garbage
			memset(&m_rayTracingParams, 0, sizeof(m_rayTracingParams));
		}
		break;
		default: SEAssertF("Invalid type");
		}

		*this = rhs;
	};


	Batch& Batch::operator=(Batch const& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_lifetime = rhs.m_lifetime;
			m_type = rhs.m_type;

			switch (m_type)
			{
			case BatchType::Raster:
			{
				m_rasterParams = rhs.m_rasterParams;
			}
			break;
			case BatchType::Compute:
			{
				m_computeParams = rhs.m_computeParams;
			}
			break;
			case BatchType::RayTracing:
			{
				m_rayTracingParams = rhs.m_rayTracingParams;
			}
			break;
			default: SEAssertF("Invalid type");
			}

			m_batchShader = rhs.m_batchShader;
			m_effectID = rhs.m_effectID;
			m_drawStyleBitmask = rhs.m_drawStyleBitmask;
			m_batchFilterBitmask = rhs.m_batchFilterBitmask;

			m_batchBuffers = rhs.m_batchBuffers;

			m_batchTextureSamplerInputs = rhs.m_batchTextureSamplerInputs;
			m_batchRWTextureInputs = rhs.m_batchRWTextureInputs;

			m_batchRootConstants = rhs.m_batchRootConstants;

			SetDataHash(rhs.GetDataHash());
		}
		return *this;
	};


	Batch::~Batch()
	{
		switch (m_type)
		{
		case BatchType::Raster:
		{
			m_rasterParams.~RasterParams();
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams.~ComputeParams();
		}
		break;
		case BatchType::RayTracing:
		{
			m_rayTracingParams.~RayTracingParams();
		}
		break;
		case BatchType::Invalid:
		{
			// Do nothing
		}
		default: SEAssertF("Invalid type");
		}
		
		// We'll let everything else be destroyed when it goes out of scope
	};


	void Batch::Destroy()
	{
		switch (m_type)
		{
		case BatchType::Raster:
		{
			m_rasterParams.~RasterParams();
		}
		break;
		case BatchType::Compute:
		{
			m_computeParams.~ComputeParams();
		}
		break;
		case BatchType::RayTracing:
		{
			m_rayTracingParams.~RayTracingParams();
		}
		break;
		default: SEAssertF("Invalid type: Was Batch already destroyed?");
		}
		m_type = BatchType::Invalid;

		m_batchShader = nullptr;
		m_effectID = 0;
		m_drawStyleBitmask = 0;
		m_batchFilterBitmask = 0;
		m_renderDataID = gr::k_invalidRenderDataID;
		
		m_batchBuffers.clear();

		m_batchTextureSamplerInputs.clear();
		m_batchRWTextureInputs.clear();
		m_batchRootConstants.Destroy();
	};


	Batch Batch::Duplicate(Batch const& rhs, re::Lifetime newLifetime)
	{
		Batch result(rhs);
		result.m_lifetime = newLifetime;

#if defined(_DEBUG)
		for (auto const& bufferInput : result.m_batchBuffers)
		{
			ValidateBufferLifetimeCompatibility(result.m_lifetime, bufferInput);
		}
#endif

		return result;
	}


	void Batch::Finalize(effect::drawstyle::Bitmask stageBitmask)
	{
		SEAssert(GetDataHash() != 0, "Batch data hash has not been computed")
		SEAssert(m_effectID != 0, "Invalid EffectID");
		SEAssert(m_batchShader == nullptr, "Batch already has a shader. This is unexpected");

		// TODO: We don't update the data hash even though we're modifying the m_drawStyleBitmask, as by this point
		// instancing has (currently) already been handled. This will probably change in future!
		m_drawStyleBitmask |= stageBitmask;
		
		m_batchShader = re::RenderManager::Get()->GetEffectDB().GetResolvedShader(m_effectID, m_drawStyleBitmask);

		SEAssert(m_type != BatchType::Raster ||
			IsBatchAndShaderTopologyCompatible(
				GetRasterParams().m_primitiveTopology,
				m_batchShader->GetRasterizationState()->GetPrimitiveTopologyType()),
			"Raster topology mode is incompatible with shader pipeline state topology type");

		// Resolve vertex input slots now that we've decided which shader will be used:
		if (m_type == BatchType::Raster)
		{
			uint8_t numVertexStreams = 0;
			bool needsRepacking = false;
			for (uint8_t i = 0; i < gr::VertexStream::k_maxVertexStreams; ++i)
			{
				// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
				if (m_rasterParams.m_vertexBuffers[i].GetStream() == nullptr)
				{
					break;
				}

				SEAssert((m_lifetime == re::Lifetime::SingleFrame) ||
					(m_rasterParams.m_vertexBuffers[i].GetStream()->GetLifetime() == re::Lifetime::Permanent &&
						m_lifetime == re::Lifetime::Permanent),
					"Cannot add a vertex stream with a single frame lifetime to a permanent batch");
				
				const gr::VertexStream::Type curStreamType = 
					m_rasterParams.m_vertexBuffers[i].m_view.m_streamView.m_type;
				
				// Find consecutive streams with the same type, and resolve the final vertex slot from the shader
				uint8_t semanticIdx = 0; // Start at 0 to ensure we process the current stream
				while (i + semanticIdx < gr::VertexStream::k_maxVertexStreams &&
					m_rasterParams.m_vertexBuffers[i + semanticIdx].GetStream() &&
					m_rasterParams.m_vertexBuffers[i + semanticIdx].m_view.m_streamView.m_type == curStreamType)
				{					
					const uint8_t vertexAttribSlot = m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					if (vertexAttribSlot != re::VertexStreamMap::k_invalidSlotIdx)
					{
						m_rasterParams.m_vertexBuffers[i + semanticIdx].m_bindSlot =
							m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					}
					else
					{
						m_rasterParams.m_vertexBuffers[i + semanticIdx].GetStream() = nullptr;
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
					if (m_rasterParams.m_vertexBuffers[i].GetStream() == nullptr)
					{
						uint8_t nextValidIdx = i + 1;
						while (nextValidIdx < numVertexStreams &&
							m_rasterParams.m_vertexBuffers[nextValidIdx].GetStream() == nullptr)
						{
							++nextValidIdx;
						}
						if (nextValidIdx < numVertexStreams &&
							m_rasterParams.m_vertexBuffers[nextValidIdx].GetStream() != nullptr)
						{
							m_rasterParams.m_vertexBuffers[i] = m_rasterParams.m_vertexBuffers[nextValidIdx];
							m_rasterParams.m_vertexBuffers[nextValidIdx] = {};
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

			ValidateVertexStreams(m_lifetime, m_rasterParams.m_vertexBuffers); // _DEBUG only
		}
	}


	void Batch::SetInstanceCount(uint32_t numInstances)
	{
		SEAssert(m_type == BatchType::Raster, "Invalid type");

		m_rasterParams.m_numInstances = numInstances;
	}


	void Batch::ComputeDataHash()
	{		
		SEAssert(GetDataHash() == 0, "Data hash already computed. This is unexpected");

		// Note: We don't consider the re::Lifetime m_lifetime, as we want single frame/permanent batches to instance

		AddDataBytesToHash(m_type);

		switch (m_type)
		{
		case BatchType::Raster:
		{
			// Note: We assume the hash is used to evaluate batch equivalence when sorting, to enable instancing. Thus,
			// we don't consider the m_batchGeometryMode or m_numInstances

			AddDataBytesToHash(m_rasterParams.m_primitiveTopology);

			for (VertexBufferInput const& vertexStream : m_rasterParams.m_vertexBuffers)
			{
				if (vertexStream.GetStream() == nullptr)
				{
					break;
				}

				AddDataBytesToHash(vertexStream.GetStream()->GetDataHash());
			}
			if (m_rasterParams.m_indexBuffer.GetStream())
			{
				AddDataBytesToHash(m_rasterParams.m_indexBuffer.GetStream()->GetDataHash());
			}

			AddDataBytesToHash(m_rasterParams.m_materialUniqueID);

			SEStaticAssert(sizeof(Batch::RasterParams) == 1112, "Must update this if RasterParams size has changed");
		}
		break;
		case BatchType::Compute:
		{
			// Instancing doesn't apply to compute shaders; m_threadGroupCount is included just as it's a differentiator
			AddDataBytesToHash(m_computeParams.m_threadGroupCount);

			SEStaticAssert(sizeof(Batch::ComputeParams) == 12, "Must update this if ComputeParams size has changed");
		}
		break;
		case BatchType::RayTracing:
		{
			SEStaticAssert(sizeof(Batch::RayTracingParams) == 80 || sizeof(Batch::RayTracingParams) == 72,
				"Must update this if RayTracingParams debug/release size has changed");

			AddDataBytesToHash(m_rayTracingParams.m_operation);
			AddDataBytesToHash(m_rayTracingParams.m_ASInput.m_shaderName);
			AddDataBytesToHash(m_rayTracingParams.m_ASInput.m_accelerationStructure->GetNameHash());
			AddDataBytesToHash(m_rayTracingParams.m_dispatchDimensions);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		// Shader:
		if (m_batchShader)
		{
			AddDataBytesToHash(m_batchShader->GetShaderIdentifier());
		}

		AddDataBytesToHash(m_effectID);
		AddDataBytesToHash(m_drawStyleBitmask);
		AddDataBytesToHash(m_batchFilterBitmask);
		
		// Note: We must consider buffers added before instancing has been calcualted, as they allow us to
		// differentiate batches that are otherwise identical. We'll use the same, identical buffer on the merged
		// instanced batches later
		for (size_t i = 0; i < m_batchBuffers.size(); i++)
		{
			AddDataBytesToHash(m_batchBuffers[i].GetBuffer()->GetUniqueID());
		}

		// Include textures/samplers in the batch hash:
		for (auto const& texSamplerInput : m_batchTextureSamplerInputs)
		{
			AddDataBytesToHash(texSamplerInput.m_texture->GetUniqueID());
			AddDataBytesToHash(texSamplerInput.m_sampler->GetUniqueID());
		}

		// Include RW textures in the batch hash:
		for (auto const& rwTexInput : m_batchRWTextureInputs)
		{
			AddDataBytesToHash(rwTexInput.m_texture->GetUniqueID());
		}

		// Root constants:
		AddDataBytesToHash(m_batchRootConstants.GetDataHash());
	}


	void Batch::SetFilterMaskBit(Filter filterBit, bool enabled)
	{
		if (enabled)
		{
			m_batchFilterBitmask |= static_cast<re::Batch::FilterBitmask>(filterBit);
		}
		else if (m_batchFilterBitmask & static_cast<re::Batch::FilterBitmask>(filterBit))
		{
			m_batchFilterBitmask ^= static_cast<re::Batch::FilterBitmask>(filterBit);
		}
	}


	bool Batch::MatchesFilterBits(re::Batch::FilterBitmask required, re::Batch::FilterBitmask excluded) const
	{
		if (required == 0 && excluded == 0) // Accept all batches by default
		{
			return true;
		}

		// Only a single bit on a Batch must match with the excluded mask for a Batch to be excluded
		const bool isExcluded = (m_batchFilterBitmask & excluded);

		// A Batch must contain all bits in the included mask to be included
		// A Batch may contain more bits than what is required, so long as it matches all required bits
		bool isFullyIncluded = false;
		if (!isExcluded)
		{
			const re::Batch::FilterBitmask invertedRequiredBits = ~required;
			const re::Batch::FilterBitmask matchingBatchBits = (m_batchFilterBitmask & invertedRequiredBits) ^ m_batchFilterBitmask;
			isFullyIncluded = (matchingBatchBits == required);
		}

		return !isExcluded && isFullyIncluded;
	}


	void Batch::SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer)
	{
		SetBuffer(re::BufferInput(shaderName, buffer));
	}


	void Batch::SetBuffer(std::string_view shaderName, std::shared_ptr<re::Buffer> const& buffer, re::BufferView const& view)
	{
		SetBuffer(re::BufferInput(shaderName, buffer, view));
	}


	void Batch::SetBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetName().empty() &&
			bufferInput.GetBuffer() != nullptr,
			"Cannot set a unnamed or null buffer");

		ValidateBufferLifetimeCompatibility(m_lifetime, bufferInput);

#if defined(_DEBUG)
		for (auto const& existingBuffer : m_batchBuffers)
		{
			SEAssert(bufferInput.GetBuffer()->GetNameHash() != existingBuffer.GetBuffer()->GetNameHash(),
				"Buffer with the same name has already been set. Re-adding it changes the data hash");
		}
#endif
		if (m_batchBuffers.empty())
		{
			m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);
		}

		m_batchBuffers.emplace_back(std::move(bufferInput));
	}


	void Batch::SetBuffer(re::BufferInput const& bufferInput)
	{
		SetBuffer(re::BufferInput(bufferInput));
	}


	void Batch::SetTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		core::InvPtr<re::Sampler> const& sampler,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(sampler.IsValid(), "Invalid sampler");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0, "Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchTextureSamplerInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName.data()) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		if (m_batchTextureSamplerInputs.empty())
		{
			m_batchTextureSamplerInputs.reserve(k_texSamplerInputReserveAmount);
		}

		m_batchTextureSamplerInputs.emplace_back(shaderName, texture, sampler, texView);
	}


	void Batch::SetRWTextureInput(
		std::string_view shaderName,
		core::InvPtr<re::Texture> const& texture,
		re::TextureView const& texView)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0 && 
			(texture->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchRWTextureInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName.data()) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		if (m_batchRWTextureInputs.empty())
		{
			m_batchRWTextureInputs.reserve(k_rwTextureInputReserveAmount);
		}

		m_batchRWTextureInputs.emplace_back(RWTextureInput{ shaderName, texture, texView });
	}
}