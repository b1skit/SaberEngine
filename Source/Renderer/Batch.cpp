// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "BufferInput.h"
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


	void ValidateBufferLifetimeCompatibility(re::Lifetime batchLifetime, re::Lifetime bufferLifetime)
	{
#if defined(_DEBUG)
		SEAssert(batchLifetime == re::Lifetime::SingleFrame ||
			(batchLifetime == re::Lifetime::Permanent && bufferLifetime == re::Lifetime::Permanent),
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
		re::Lifetime batchLifetime, re::Batch::VertexBufferInput* vertexBuffers, uint8_t numVertexBuffers)
	{
#if defined(_DEBUG)

		std::unordered_set<uint8_t> seenSlots;
		seenSlots.reserve(re::VertexStream::k_maxVertexStreams);

		SEAssert(vertexBuffers[0].m_vertexBuffer, "Must have at least 1 non-null vertex stream");
		
		bool seenNull = false;
		for (size_t i = 0; i < numVertexBuffers; ++i)
		{
			if (vertexBuffers[i].m_vertexBuffer == nullptr)
			{
				seenNull = true;
			}
			SEAssert(!seenNull || vertexBuffers[i].m_vertexBuffer == nullptr,
				"Found a non-null entry after a null. Vertex streams must be tightly packed");

			if (vertexBuffers[i].m_vertexBuffer)
			{
				ValidateVertexStreamLifetime(batchLifetime, vertexBuffers[i].m_vertexBuffer->GetLifetime());
			}

			SEAssert(vertexBuffers[i].m_vertexBuffer == nullptr || 
				vertexBuffers[i].m_bindSlot != re::Batch::VertexBufferInput::k_invalidSlotIdx,
				"Invalid bind slot detected");

			SEAssert(vertexBuffers[i].m_vertexBuffer == nullptr ||
				i + 1 == numVertexBuffers ||
				vertexBuffers[i + 1].m_vertexBuffer == nullptr ||
				(vertexBuffers[i].m_vertexBuffer->GetBufferParams().m_vertexStreamView.m_type < 
					vertexBuffers[i + 1].m_vertexBuffer->GetBufferParams().m_vertexStreamView.m_type) ||
				vertexBuffers[i].m_bindSlot + 1 == vertexBuffers[i + 1].m_bindSlot,
				"Vertex streams of the same type must be stored in monotoically-increasing slot order");

			if (vertexBuffers[i].m_vertexBuffer != nullptr)
			{
				SEAssert(!seenSlots.contains(vertexBuffers[i].m_bindSlot), "Duplicate slot index detected");
				seenSlots.emplace(vertexBuffers[i].m_bindSlot);
			}
		}

#endif
	}


	re::Shader const* GetResolvedShader(EffectID effectID, effect::drawstyle::Bitmask drawStyleBitmask)
	{
		SEAssert(effectID.IsValid(), "Invalid Effect");

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
			bitmask |= effect::drawstyle::MorphTargets_Enabled;
		}

		return bitmask;
	}


	bool IsBatchAndShaderTopologyCompatible(
		gr::MeshPrimitive::PrimitiveTopology topologyMode, re::PipelineState::PrimitiveTopologyType topologyType)
	{
		switch (topologyType)
		{
		case re::PipelineState::PrimitiveTopologyType::Point:
		{
			return topologyMode == gr::MeshPrimitive::PrimitiveTopology::PointList;
		}
		break;
		case re::PipelineState::PrimitiveTopologyType::Line:
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
		case re::PipelineState::PrimitiveTopologyType::Triangle:
		{
			return topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleList ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStrip ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency ||
				topologyMode == gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency;
		}
		break;
		case re::PipelineState::PrimitiveTopologyType::Patch:
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
			.m_primitiveTopology = meshPrimitive->GetMeshParams().m_primitiveTopology,
			.m_numVertexBuffers = 0,
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
			SEAssert(vertexStreams[slotIdx].m_vertexStream->GetBuffer(), "Buffer shouldn't be null here");

			m_graphicsParams.m_vertexBuffers[slotIdx] = VertexBufferInput{
				.m_vertexBuffer = vertexStreams[slotIdx].m_vertexStream->GetBuffer(),
				.m_bindSlot = VertexBufferInput::k_invalidSlotIdx, // Populated during shader resolve
			};
			m_graphicsParams.m_numVertexBuffers++;
		}
		m_graphicsParams.m_indexBuffer = meshPrimitive->GetIndexStream()->GetBuffer();

		ComputeDataHash();
	}


	Batch::Batch(
		re::Lifetime lifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		gr::Material::MaterialInstanceRenderData const* materialInstanceData)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_batchShader(nullptr)
		, m_effectID(materialInstanceData ? materialInstanceData->m_effectID : util::StringHash(/*Invalid*/))
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_primitiveTopology = meshPrimRenderData.m_meshPrimitiveParams.m_primitiveTopology,
			.m_numVertexBuffers = 0,
		};

		// We assume the MeshPrimitive's vertex streams are ordered such that identical stream types are tightly
		// packed, and in the correct channel order corresponding to the final shader slots (e.g. uv0, uv1, etc)
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(meshPrimRenderData.m_numVertexStreams); slotIdx++)
		{
			if (meshPrimRenderData.m_vertexStreams[slotIdx] == nullptr)
			{
				break;
			}
			SEAssert(meshPrimRenderData.m_vertexStreams[slotIdx]->GetBuffer(), "Buffer shouldn't be null here");

			SEAssert((m_lifetime == re::Lifetime::SingleFrame) ||
				(meshPrimRenderData.m_vertexStreams[slotIdx]->GetLifetime() == 
					re::Lifetime::Permanent && 
					m_lifetime == re::Lifetime::Permanent),
				"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

			m_graphicsParams.m_vertexBuffers[slotIdx] = VertexBufferInput{
				.m_vertexBuffer = meshPrimRenderData.m_vertexStreams[slotIdx]->GetBuffer(),
				.m_bindSlot = VertexBufferInput::k_invalidSlotIdx, // Populated during shader resolve
			};
			m_graphicsParams.m_numVertexBuffers++;
		}
		m_graphicsParams.m_indexBuffer = meshPrimRenderData.m_indexStream->GetBuffer();
		
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

		// Add the morph data buffer (it'll be considered when the batch hash is computed)
		if (meshPrimRenderData.m_hasMorphTargets)
		{
			SetBuffer(s_interleavedMorphDataShaderName, meshPrimRenderData.m_interleavedMorphData);
		}

		m_drawStyleBitmask = ComputeBatchBitmask(materialInstanceData, meshPrimRenderData.m_hasMorphTargets);

		ComputeDataHash();
	}


	Batch::Batch(
		re::Lifetime lifetime,
		GraphicsParams const& graphicsParams, 
		EffectID effectID,
		effect::drawstyle::Bitmask bitmask)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_batchShader(nullptr)
		, m_effectID(effectID)
		, m_drawStyleBitmask(bitmask)
		, m_batchFilterBitmask(0)
	{
		SEAssert(graphicsParams.m_numVertexBuffers > 0, "Can't have a graphics batch with 0 vertex streams");

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
			ValidateBufferLifetimeCompatibility(result.m_lifetime, buf.GetBuffer()->GetLifetime());
		}
#endif

		return result;
	}


	void Batch::ResolveShader(effect::drawstyle::Bitmask stageBitmask)
	{
		SEAssert(m_effectID.IsValid(), "Invalid EffectID");
		SEAssert(m_batchShader == nullptr, "Batch already has a shader. This is unexpected");

		// TODO: We don't update the data hash even though we're modifying the m_drawStyleBitmask, as by this point
		// instancing has (currently) already been handled. This will probably change in future!
		m_drawStyleBitmask |= stageBitmask;
		
		m_batchShader = GetResolvedShader(m_effectID, m_drawStyleBitmask);

		SEAssert(m_type != BatchType::Graphics ||
			IsBatchAndShaderTopologyCompatible(
				GetGraphicsParams().m_primitiveTopology,
				m_batchShader->GetPipelineState()->GetPrimitiveTopologyType()),
			"Graphics topology mode is incompatible with shader pipeline state topology type");

		// Resolve vertex input slots now that we've decided which shader will be used:
		if (m_type == BatchType::Graphics)
		{
			bool needsRepacking = false;
			for (uint8_t i = 0; i < m_graphicsParams.m_numVertexBuffers; ++i)
			{
				// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
				const re::VertexStream::Type curStreamType = 
					m_graphicsParams.m_vertexBuffers[i].m_vertexBuffer->GetBufferParams().m_vertexStreamView.m_type;

				// Find consecutive streams with the same type, and resolve the final vertex slot from the shader
				uint8_t semanticIdx = 0;
				while (i + semanticIdx < m_graphicsParams.m_numVertexBuffers &&
					m_graphicsParams.m_vertexBuffers[i + semanticIdx].m_vertexBuffer &&
					m_graphicsParams.m_vertexBuffers[i + semanticIdx].m_vertexBuffer
						->GetBufferParams().m_vertexStreamView.m_type == curStreamType)
				{				
					const uint8_t vertexAttribSlot = m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					if (vertexAttribSlot != re::VertexStreamMap::k_invalidSlotIdx)
					{
						m_graphicsParams.m_vertexBuffers[i + semanticIdx].m_bindSlot =
							m_batchShader->GetVertexAttributeSlot(curStreamType, semanticIdx);
					}
					else
					{
						m_graphicsParams.m_vertexBuffers[i + semanticIdx].m_vertexBuffer = nullptr;
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
				const uint8_t curNumStreams = m_graphicsParams.m_numVertexBuffers;
				uint8_t numValidStreams = 0;
				for (uint8_t i = 0; i < curNumStreams; ++i)
				{
					if (m_graphicsParams.m_vertexBuffers[i].m_vertexBuffer == nullptr)
					{
						uint8_t nextValidIdx = i + 1;
						while (nextValidIdx < curNumStreams && 
							m_graphicsParams.m_vertexBuffers[nextValidIdx].m_vertexBuffer == nullptr)
						{
							++nextValidIdx;
						}
						if (nextValidIdx < curNumStreams && 
							m_graphicsParams.m_vertexBuffers[nextValidIdx].m_vertexBuffer != nullptr)
						{
							m_graphicsParams.m_vertexBuffers[i] = m_graphicsParams.m_vertexBuffers[nextValidIdx];
							m_graphicsParams.m_vertexBuffers[nextValidIdx] = {};
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
				m_graphicsParams.m_numVertexBuffers = numValidStreams;
			}

			ValidateVertexStreams(m_lifetime, m_graphicsParams.m_vertexBuffers, m_graphicsParams.m_numVertexBuffers); // _DEBUG only
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

			AddDataBytesToHash(m_graphicsParams.m_primitiveTopology);

			for (VertexBufferInput const& vertexStream : m_graphicsParams.m_vertexBuffers)
			{
				if (vertexStream.m_vertexBuffer == nullptr)
				{
					break;
				}

				// Note: The the data hash of our vertex stream buffer would be preferable here, but the UniqueID works
				// as well as we maintain unique vertex streams only
				AddDataBytesToHash(vertexStream.m_vertexBuffer->GetUniqueID());
			}
			if (m_graphicsParams.m_indexBuffer)
			{
				// Note: As above - The the data hash of our vertex stream buffer would be preferable here, but the
				// UniqueID works as well as we maintain unique vertex streams only
				AddDataBytesToHash(m_graphicsParams.m_indexBuffer->GetUniqueID());
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
			AddDataBytesToHash(m_batchBuffers[i].GetBuffer()->GetUniqueID());
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


	void Batch::SetBuffer(std::string const& shaderName, std::shared_ptr<re::Buffer> buffer)
	{
		SetBuffer(re::BufferInput(shaderName, buffer));
	}


	void Batch::SetBuffer(re::BufferInput const& bufferInput)
	{
		SetBuffer(re::BufferInput(bufferInput));
	}


	void Batch::SetBuffer(re::BufferInput&& bufferInput)
	{
		SEAssert(!bufferInput.GetName().empty() &&
			bufferInput.GetBuffer() != nullptr,
			"Cannot set a unnamed or null buffer");

		ValidateBufferLifetimeCompatibility(m_lifetime, bufferInput.GetBuffer()->GetLifetime());

#if defined(_DEBUG)
		for (auto const& existingBuffer : m_batchBuffers)
		{
			SEAssert(bufferInput.GetBuffer()->GetNameHash() != existingBuffer.GetBuffer()->GetNameHash(),
				"Buffer with the same name has already been set. Re-adding it changes the data hash");
		}
#endif
		AddDataBytesToHash(bufferInput.GetBuffer()->GetUniqueID());

		m_batchBuffers.emplace_back(std::move(bufferInput));
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