// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "Material.h"
#include "RenderManager.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"


namespace
{
	constexpr size_t k_batchBufferIDsReserveAmount = 10;


	void ValidateBufferLifetimeCompatibility(re::Lifetime liftime, re::Buffer::Type bufferType)
	{
#if defined(_DEBUG)
		SEAssert(liftime == re::Lifetime::SingleFrame ||
			(liftime == re::Lifetime::Permanent &&
			(bufferType == re::Buffer::Type::Mutable || bufferType == re::Buffer::Type::Immutable)),
			"Trying to set a buffer with a mismatching lifetime. Permanent batches cannot (currently) hold "
			"single frame buffers, as they'd incorrectly maintain their life beyond the frame. Single frame "
			"batches can hold any type of buffers (but should not be responsible for the lifetime of a "
			"permanent buffer as they're expensive to create/destroy)");
#endif
	}


	void ValidateVertexStreamLifetime(re::Lifetime batchLifetime, re::VertexStream const* vertexStream)
	{
#if defined(_DEBUG)
		SEAssert(batchLifetime == re::Lifetime::SingleFrame ||
				(batchLifetime == re::Lifetime::Permanent &&
					vertexStream->GetLifetime() == re::Lifetime::Permanent),
			"Cannot add a vertex stream with a single frame lifetime to a permanent batch");
#endif
	}


	void ValidateVertexStreams(
		re::Lifetime batchLifetime, re::Batch::VertexStreamInput* vertexStreams, uint8_t numVertexStreams)
	{
#if defined(_DEBUG)

		std::unordered_set<uint8_t> seenSlots;
		seenSlots.reserve(re::VertexStream::k_maxVertexStreams);

		SEAssert(vertexStreams[0].m_vertexStream, "Must have at least 1 non-null vertex stream");
		
		bool seenNull = false;
		for (size_t i = 0; i < numVertexStreams; ++i)
		{
			if (vertexStreams[i].m_vertexStream == nullptr)
			{
				seenNull = true;
			}
			SEAssert(!seenNull || vertexStreams[i].m_vertexStream == nullptr,
				"Found a non-null entry after a null. Vertex streams must be tightly packed");

			if (vertexStreams[i].m_vertexStream)
			{
				ValidateVertexStreamLifetime(batchLifetime, vertexStreams[i].m_vertexStream);
			}

			SEAssert(vertexStreams[i].m_vertexStream == nullptr || 
				vertexStreams[i].m_bindSlot != re::Batch::VertexStreamInput::k_invalidSlotIdx,
				"Invalid bind slot detected");

			SEAssert(vertexStreams[i].m_vertexStream == nullptr ||
				i + 1 == numVertexStreams ||
				vertexStreams[i + 1].m_vertexStream == nullptr ||
				vertexStreams[i].m_vertexStream->GetType() < vertexStreams[i + 1].m_vertexStream->GetType() ||
				vertexStreams[i].m_bindSlot + 1 == vertexStreams[i + 1].m_bindSlot,
				"Vertex streams of the same type must be stored in monotoically-increasing slot order");

			if (vertexStreams[i].m_vertexStream != nullptr)
			{
				SEAssert(!seenSlots.contains(vertexStreams[i].m_bindSlot), "Duplicate slot index detected");
				seenSlots.emplace(vertexStreams[i].m_bindSlot);
			}
		}

#endif
	}


	re::Shader const* GetResolvedShader(EffectID effectID, effect::drawstyle::Bitmask drawStyleBitmask)
	{
		SEAssert(effectID != effect::Effect::k_invalidEffectID, "Invalid Effect");

		effect::Effect const* effect = re::RenderManager::Get()->GetEffectDB().GetEffect(effectID);
		effect::Technique const* technique = effect->GetResolvedTechnique(drawStyleBitmask);
		return technique->GetShader();
	}


	effect::drawstyle::Bitmask ComputeBatchBitmask(
		gr::Material::MaterialInstanceRenderData const* materialInstanceData,
		bool hasMorphTargets)
	{
		effect::drawstyle::Bitmask bitmask = 0;

		if (materialInstanceData)
		{
			// Alpha mode:
			switch (materialInstanceData->m_alphaMode)
			{
			case gr::Material::AlphaMode::Opaque:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Opaque;
			}
			break;
			case gr::Material::AlphaMode::Mask:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Mask;
			}
			break;
			case gr::Material::AlphaMode::Blend:
			{
				bitmask |= effect::drawstyle::MaterialAlphaMode_Blend;
			}
			break;
			default:
				SEAssertF("Invalid Material AlphaMode");
			}

			// Material sidedness:
			bitmask |= materialInstanceData->m_isDoubleSided ?
				effect::drawstyle::MaterialSidedness_Double : effect::drawstyle::MaterialSidedness_Single;
		}

		if (hasMorphTargets)
		{
			// TODO: Implement this correctly. For now, we just use a single morph target drawstyle
			bitmask |= effect::drawstyle::MorphTargets_Pos8;
		}

		return bitmask;
	}


	bool IsBatchAndShaderTopologyCompatible(
		gr::MeshPrimitive::TopologyMode topologyMode, re::PipelineState::TopologyType topologyType)
	{
		// Note: These rules are not complete. If you fail this assert, it's possible you're in a valid state. The goal
		// is to catch unintended accidents
		switch (topologyType)
		{
		case re::PipelineState::TopologyType::Point:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::PointList;
		}
		break;
		case re::PipelineState::TopologyType::Line:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::LineList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::LineStripAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency;
		}
		break;
		case re::PipelineState::TopologyType::Triangle:
		{
			return topologyMode == gr::MeshPrimitive::TopologyMode::TriangleList ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency;
		}
		break;
		case re::PipelineState::TopologyType::Patch:
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
	Batch::Batch(
		re::Lifetime lifetime,
		gr::MeshPrimitive const* meshPrimitive,
		EffectID effectID)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_batchShader(nullptr)
		, m_effectID(effectID)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_batchTopologyMode = meshPrimitive->GetMeshParams().m_topologyMode,
			.m_numVertexStreams = 0,
		};

		// We assume the meshPrimitive's vertex streams are ordered such that identical stream types are tightly
		// packed, and in the correct channel order corresponding to the final shader slots (e.g. uv0, uv1, etc)
		std::vector<gr::MeshPrimitive::MeshVertexStream> const& vertexStreams = meshPrimitive->GetVertexStreams();
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(vertexStreams.size()); slotIdx++)
		{
			if (vertexStreams[slotIdx].m_vertexStream == nullptr)
			{
				break;
			}

			SEAssert((m_lifetime == re::Lifetime::SingleFrame) ||
				(vertexStreams[slotIdx].m_vertexStream->GetLifetime() == re::Lifetime::Permanent &&
					m_lifetime == re::Lifetime::Permanent),
				"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

			m_graphicsParams.m_vertexStreams[slotIdx] = VertexStreamInput{
				.m_vertexStream = vertexStreams[slotIdx].m_vertexStream,
				.m_bindSlot = VertexStreamInput::k_invalidSlotIdx, // Populated during shader resolve
			};
			m_graphicsParams.m_numVertexStreams++;
		}
		m_graphicsParams.m_indexStream = meshPrimitive->GetIndexStream();

		ComputeDataHash();
	}


	Batch::Batch(
		re::Lifetime lifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		gr::Material::MaterialInstanceRenderData const* materialInstanceData)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_batchShader(nullptr)
		, m_effectID(materialInstanceData ? materialInstanceData->m_materialEffectID : effect::Effect::k_invalidEffectID)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_batchTopologyMode = meshPrimRenderData.m_meshPrimitiveParams.m_topologyMode,
			.m_numVertexStreams = 0,
		};

		// We assume the MeshPrimitive's vertex streams are ordered such that identical stream types are tightly
		// packed, and in the correct channel order corresponding to the final shader slots (e.g. uv0, uv1, etc)
		bool hasMorphTargets = false;
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(meshPrimRenderData.m_numVertexStreams); slotIdx++)
		{
			if (meshPrimRenderData.m_vertexStreams[slotIdx] == nullptr)
			{
				break;
			}

			SEAssert((m_lifetime == re::Lifetime::SingleFrame) ||
				(meshPrimRenderData.m_vertexStreams[slotIdx]->GetLifetime() == 
					re::Lifetime::Permanent && 
					m_lifetime == re::Lifetime::Permanent),
				"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

			if (meshPrimRenderData.m_vertexStreams[slotIdx]->IsMorphData())
			{
				hasMorphTargets = true;
			}

			m_graphicsParams.m_vertexStreams[slotIdx] = VertexStreamInput{
				.m_vertexStream = meshPrimRenderData.m_vertexStreams[slotIdx],
				.m_bindSlot = VertexStreamInput::k_invalidSlotIdx, // Populated during shader resolve
			};
			m_graphicsParams.m_numVertexStreams++;
		}
		m_graphicsParams.m_indexStream = meshPrimRenderData.m_indexStream;
		
		// Material textures/samplers:
		if (materialInstanceData)
		{
			SEAssert(materialInstanceData->m_textures.size() == materialInstanceData->m_samplers.size(),
				"Texture/sampler array size mismatch. We assume the all material instance arrays are the same size");

			for (size_t i = 0; i < materialInstanceData->m_textures.size(); i++)
			{
				if (materialInstanceData->m_textures[i] && materialInstanceData->m_samplers[i])
				{
					AddTextureInput(
						materialInstanceData->m_shaderSamplerNames[i],
						materialInstanceData->m_textures[i],
						materialInstanceData->m_samplers[i],
						re::TextureView(materialInstanceData->m_textures[i]));
				}
			}

			m_graphicsParams.m_materialUniqueID = materialInstanceData->m_srcMaterialUniqueID;

			// Filter bits:
			SetFilterMaskBit(Filter::AlphaBlended, materialInstanceData->m_alphaMode == gr::Material::AlphaMode::Blend);
			SetFilterMaskBit(Filter::CastsShadow, materialInstanceData->m_isShadowCaster);
		}

		m_drawStyleBitmask = ComputeBatchBitmask(materialInstanceData, hasMorphTargets);

		ComputeDataHash();
	}


	Batch::Batch(
		re::Lifetime lifetime,
		GraphicsParams const& graphicsParams, 
		EffectID effectID)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_batchShader(nullptr)
		, m_effectID(effectID)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		SEAssert(graphicsParams.m_numVertexStreams > 0, "Can't have a graphics batch with 0 vertex streams");

		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = graphicsParams;

		ComputeDataHash();
	}


	Batch::Batch(
		re::Lifetime lifetime,
		ComputeParams const& computeParams, 
		EffectID effectID)
		: m_lifetime(lifetime)
		, m_type(BatchType::Compute)
		, m_computeParams(computeParams)
		, m_batchShader(nullptr)
		, m_effectID(effectID)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
	}


	Batch Batch::Duplicate(Batch const& rhs, re::Lifetime newLifetime)
	{
		Batch result = rhs;
		result.m_lifetime = newLifetime;

#if defined(_DEBUG)
		for (auto const& buf : result.m_batchBuffers)
		{
			ValidateBufferLifetimeCompatibility(result.m_lifetime, buf->GetType());
		}
#endif

		return result;
	}


	void Batch::ResolveShader(effect::drawstyle::Bitmask stageBitmask)
	{
		SEAssert(m_effectID != effect::Effect::k_invalidEffectID, "Invalid EffectID");
		SEAssert(m_batchShader == nullptr, "Batch already has a shader. This is unexpected");

		// TODO: We don't update the data hash even though we're modifying the m_drawStyleBitmask, as by this point
		// instancing has (currently) already been handled. This will probably change in future!
		m_drawStyleBitmask |= stageBitmask;
		
		m_batchShader = GetResolvedShader(m_effectID, m_drawStyleBitmask);

		SEAssert(m_type != BatchType::Graphics ||
			IsBatchAndShaderTopologyCompatible(
				GetGraphicsParams().m_batchTopologyMode,
				m_batchShader->GetPipelineState()->GetTopologyType()),
			"Graphics topology mode is incompatible with shader pipeline state topology type");

		// Resolve vertex input slots now that we've decided which shader will be used:
		if (m_type == BatchType::Graphics)
		{
			bool needsRepacking = false;
			for (uint8_t i = 0; i < m_graphicsParams.m_numVertexStreams; ++i)
			{
				// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
				const re::VertexStream::Type curStreamType = 
					m_graphicsParams.m_vertexStreams[i].m_vertexStream->GetType();

				// Find consecutive streams with the same type, and resolve the final vertex slot from the shader
				uint8_t semanticIdx = 0;
				while (i + semanticIdx < m_graphicsParams.m_numVertexStreams &&
					m_graphicsParams.m_vertexStreams[i + semanticIdx].m_vertexStream &&
					m_graphicsParams.m_vertexStreams[i + semanticIdx].m_vertexStream->GetType() == curStreamType)
				{				
					const uint8_t vertexAttribSlot = m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					if (vertexAttribSlot != re::VertexStreamMap::k_invalidSlotIdx)
					{
						m_graphicsParams.m_vertexStreams[i + semanticIdx].m_bindSlot =
							m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					}
					else
					{
						m_graphicsParams.m_vertexStreams[i + semanticIdx].m_vertexStream = nullptr;
						needsRepacking = true;
					}
					++semanticIdx;
				}
				if (semanticIdx > 1) // Skip ahead: We've already handled all consecutive streams of the same type
				{
					i = i + (semanticIdx - 1); // -1 b/c of the last ++semanticIdx;
				}
			}

			if (needsRepacking)
			{
				const uint8_t curNumStreams = m_graphicsParams.m_numVertexStreams;
				uint8_t numValidStreams = 0;
				for (uint8_t i = 0; i < curNumStreams; ++i)
				{
					if (m_graphicsParams.m_vertexStreams[i].m_vertexStream == nullptr)
					{
						uint8_t nextValidIdx = i + 1;
						while (nextValidIdx < curNumStreams && 
							m_graphicsParams.m_vertexStreams[nextValidIdx].m_vertexStream == nullptr)
						{
							++nextValidIdx;
						}
						if (nextValidIdx < curNumStreams && 
							m_graphicsParams.m_vertexStreams[nextValidIdx].m_vertexStream != nullptr)
						{
							m_graphicsParams.m_vertexStreams[i] = m_graphicsParams.m_vertexStreams[nextValidIdx];
							m_graphicsParams.m_vertexStreams[nextValidIdx] = {};
							++numValidStreams;
						}
						else if (nextValidIdx == curNumStreams)
						{
							break; // Didn't find anything valid in the remaining elements, no point continuing
						}
					}
					else
					{
						++numValidStreams;					
					}
				}
				m_graphicsParams.m_numVertexStreams = numValidStreams;
			}

			ValidateVertexStreams(m_lifetime, m_graphicsParams.m_vertexStreams, m_graphicsParams.m_numVertexStreams); // _DEBUG only
		}
	}


	void Batch::SetInstanceCount(uint32_t numInstances)
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");

		m_graphicsParams.m_numInstances = numInstances;
	}


	void Batch::ComputeDataHash()
	{		
		ResetDataHash();

		// Note: We don't consider the re::Lifetime m_lifetime, as we want single frame/permanent batches to instance

		AddDataBytesToHash(m_type);

		switch (m_type)
		{
		case BatchType::Graphics:
		{
			// Note: We assume the hash is used to evaluate batch equivalence when sorting, to enable instancing. Thus,
			// we don't consider the m_batchGeometryMode or m_numInstances

			AddDataBytesToHash(m_graphicsParams.m_batchTopologyMode);

			for (VertexStreamInput const& vertexStream : m_graphicsParams.m_vertexStreams)
			{
				if (vertexStream.m_vertexStream == nullptr)
				{
					break;
				}
				AddDataBytesToHash(vertexStream.m_vertexStream->GetDataHash());
			}
			if (m_graphicsParams.m_indexStream)
			{
				AddDataBytesToHash(m_graphicsParams.m_indexStream->GetDataHash());
			}

			AddDataBytesToHash(m_graphicsParams.m_materialUniqueID);
		}
		break;
		case BatchType::Compute:
		{
			// Instancing doesn't apply to compute shaders; m_threadGroupCount is included just as it's a differentiator
			AddDataBytesToHash(m_computeParams.m_threadGroupCount);
		}
		break;
		default:
			SEAssertF("Invalid type");
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
			AddDataBytesToHash(m_batchBuffers[i]->GetUniqueID());
		}

		// Note: We don't compute hashes for batch textures/samplers here; they're appended as they're added
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


	void Batch::SetBuffer(std::shared_ptr<re::Buffer> buffer)
	{
		SEAssert(buffer != nullptr, "Cannot set a null buffer");

		ValidateBufferLifetimeCompatibility(m_lifetime, buffer->GetType());

#if defined(_DEBUG)
		for (auto const& existingBuffer : m_batchBuffers)
		{
			SEAssert(buffer->GetNameID() != existingBuffer->GetNameID(),
				"Buffer with the same name has already been set. Re-adding it changes the data hash");
		}
#endif
		AddDataBytesToHash(buffer->GetUniqueID());

		m_batchBuffers.emplace_back(buffer);
	}


	void Batch::AddTextureInput(
		char const* shaderName,
		re::Texture const* texture,
		re::Sampler const* sampler,
		re::TextureView const& texView)
	{
		SEAssert(shaderName != nullptr && strlen(shaderName) > 0, "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(sampler != nullptr, "Invalid sampler");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0, "Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchTextureSamplerInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		m_batchTextureSamplerInputs.emplace_back(
			TextureAndSamplerInput{ shaderName, texture, sampler, texView });

		// Include textures/samplers in the batch hash:
		AddDataBytesToHash(texture->GetUniqueID());
		AddDataBytesToHash(sampler->GetUniqueID());
	}


	void Batch::AddTextureInput(
		char const* shaderName,
		std::shared_ptr<re::Texture const> texture,
		std::shared_ptr<re::Sampler const> sampler,
		re::TextureView const& texView)
	{
		AddTextureInput(shaderName, texture.get(), sampler.get(), texView);
	}


	void Batch::AddRWTextureInput(
		char const* shaderName,
		re::Texture const* texture,
		re::TextureView const& texView)
	{
		SEAssert(shaderName != nullptr && strlen(shaderName) > 0, "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(texView.m_viewDimension != re::Texture::Dimension_Invalid, "Invalid view dimension");
		SEAssert((texture->GetTextureParams().m_usage & re::Texture::ColorSrc) != 0 && 
			(texture->GetTextureParams().m_usage & re::Texture::ColorTarget) != 0,
			"Invalid usage");

#if defined(_DEBUG)
		for (auto const& existingTexAndSamplerInput : m_batchRWTextureInputs)
		{
			SEAssert(existingTexAndSamplerInput.m_texture != texture ||
				strcmp(existingTexAndSamplerInput.m_shaderName.c_str(), shaderName) != 0,
				"This Texture has already been added with the same shader name. Re-adding it changes the data hash");
		}
#endif

		m_batchRWTextureInputs.emplace_back(
			RWTextureInput{ shaderName, texture, texView });

		// Include RW textures in the batch hash:
		AddDataBytesToHash(texture->GetUniqueID());
	}


	void Batch::AddRWTextureInput(
		char const* shaderName,
		std::shared_ptr<re::Texture const> texture,
		re::TextureView const& texView)
	{
		AddRWTextureInput(shaderName, texture.get(),  texView);
	}
}