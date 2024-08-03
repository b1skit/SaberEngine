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


	void ValidateLifetimeCompatibility(re::Batch::Lifetime liftime, re::Buffer::Type bufferType)
	{
		SEAssert(liftime == re::Batch::Lifetime::SingleFrame ||
			(liftime == re::Batch::Lifetime::Permanent &&
			(bufferType == re::Buffer::Type::Mutable || bufferType == re::Buffer::Type::Immutable)),
			"Trying to set a buffer with a mismatching lifetime. Permanent batches cannot (currently) hold "
			"single frame buffers, as they'd incorrectly maintain their life beyond the frame. Single frame "
			"batches can hold any type of buffers (but should not be responsible for the lifetime of a "
			"permanent buffer as they're expensive to create/destroy)");
	}


	re::Shader const* GetResolvedShader(EffectID effectID, effect::DrawStyle::Bitmask drawStyleBitmask)
	{
		SEAssert(effectID != effect::Effect::k_invalidEffectID, "Invalid Effect");

		effect::Effect const* effect = re::RenderManager::Get()->GetEffectDB().GetEffect(effectID);
		effect::Technique const* technique = effect->GetResolvedTechnique(drawStyleBitmask);
		return technique->GetShader().get();
	}


	effect::DrawStyle::Bitmask ComputeBatchBitmask(gr::Material::MaterialInstanceData const& materialInstanceData)
	{
		effect::DrawStyle::Bitmask bitmask = 0;

		// Alpha mode:
		switch (materialInstanceData.m_alphaMode)
		{
		case gr::Material::AlphaMode::Opaque:
		{
			bitmask |= effect::DrawStyle::AlphaMode_Opaque;
		}
		break;
		case gr::Material::AlphaMode::Mask:
		{
			bitmask |= effect::DrawStyle::AlphaMode_Mask;
		}
		break;
		case gr::Material::AlphaMode::Blend:
		{
			bitmask |= effect::DrawStyle::AlphaMode_Blend;
		}
		break;
		default:
			SEAssertF("Invalid Material AlphaMode");
		}

		// Material sidedness:
		bitmask |= materialInstanceData.m_isDoubleSided ? 
			effect::DrawStyle::MaterialSidedness_Double : effect::DrawStyle::MaterialSidedness_Single;

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
		Lifetime lifetime,
		gr::MeshPrimitive const* meshPrimitive,
		EffectID effectID)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
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
		};

		std::vector<re::VertexStream const*> const& vertexStreams = meshPrimitive->GetVertexStreams();
		memset(&m_graphicsParams.m_vertexStreams,
			0,
			m_graphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(vertexStreams.size()); slotIdx++)
		{
			if (vertexStreams[slotIdx])
			{
				SEAssert((m_lifetime == Lifetime::SingleFrame) ||
					(vertexStreams[slotIdx]->GetLifetime() == re::VertexStream::Lifetime::Permanent &&
						m_lifetime == Lifetime::Permanent),
					"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

				m_graphicsParams.m_vertexStreams[slotIdx] = vertexStreams[slotIdx];
			}
		}
		m_graphicsParams.m_indexStream = meshPrimitive->GetIndexStream();

		ComputeDataHash();
	}


	Batch::Batch(
		Lifetime lifetime, 
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		gr::Material::MaterialInstanceData const* materialInstanceData)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
		, m_batchShader(nullptr)
		, m_effectID(materialInstanceData ? materialInstanceData->m_materialEffectID : effect::Effect::k_invalidEffectID)
		, m_drawStyleBitmask(materialInstanceData ? ComputeBatchBitmask(*materialInstanceData) : 0)
		, m_batchFilterBitmask(0)
	{
		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_batchTopologyMode = meshPrimRenderData.m_meshPrimitiveParams.m_topologyMode,
		};

		// Zero out our vertex streams array:
		memset(&m_graphicsParams.m_vertexStreams,
			0,
			m_graphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));

		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(meshPrimRenderData.m_vertexStreams.size()); slotIdx++)
		{
			if (meshPrimRenderData.m_vertexStreams[slotIdx])
			{
				SEAssert((m_lifetime == Lifetime::SingleFrame) ||
					(meshPrimRenderData.m_vertexStreams[slotIdx]->GetLifetime() == 
						re::VertexStream::Lifetime::Permanent && 
						m_lifetime == Lifetime::Permanent),
					"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

				m_graphicsParams.m_vertexStreams[slotIdx] = meshPrimRenderData.m_vertexStreams[slotIdx];
			}
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

		ComputeDataHash();
	}


	Batch::Batch(
		Lifetime lifetime,
		GraphicsParams const& graphicsParams, 
		EffectID effectID)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
		, m_batchShader(nullptr)
		, m_effectID(effectID)
		, m_drawStyleBitmask(0)
		, m_batchFilterBitmask(0)
	{
		m_batchBuffers.reserve(k_batchBufferIDsReserveAmount);

		m_graphicsParams = graphicsParams;

#if defined(_DEBUG)
		for (uint8_t slotIdx = 0; slotIdx < gr::MeshPrimitive::Slot_Count; slotIdx++)
		{
			SEAssert((m_lifetime == Lifetime::SingleFrame) ||
				m_graphicsParams.m_vertexStreams[slotIdx] == nullptr ||
				(m_graphicsParams.m_vertexStreams[slotIdx]->GetLifetime() == re::VertexStream::Lifetime::Permanent && 
					m_lifetime == Lifetime::Permanent),
				"Cannot add a vertex stream with a single frame lifetime to a permanent batch");
		}
#endif

		ComputeDataHash();
	}


	Batch::Batch(
		Lifetime lifetime, 
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


	Batch Batch::Duplicate(
		Batch const& rhs,
		re::Batch::Lifetime newLifetime)
	{
		Batch result = rhs;
		result.m_lifetime = newLifetime;

#if defined(_DEBUG)
		for (auto const& buf : result.m_batchBuffers)
		{
			ValidateLifetimeCompatibility(result.m_lifetime, buf->GetType());
		}
#endif

		return result;
	}


	void Batch::ResolveShader(effect::DrawStyle::Bitmask stageBitmask)
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
	}


	void Batch::SetInstanceCount(uint32_t numInstances)
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");

		m_graphicsParams.m_numInstances = numInstances;
	}


	void Batch::ComputeDataHash()
	{		
		ResetDataHash();

		// Note: We don't consider the Batch::Lifetime m_lifetime, as we want single frame/permanent batches to instance

		AddDataBytesToHash(m_type);

		switch (m_type)
		{
		case BatchType::Graphics:
		{
			// Note: We assume the hash is used to evaluate batch equivalence when sorting, to enable instancing. Thus,
			// we don't consider the m_batchGeometryMode or m_numInstances

			AddDataBytesToHash(m_graphicsParams.m_batchTopologyMode);

			for (re::VertexStream const* vertexStream : m_graphicsParams.m_vertexStreams)
			{
				if (vertexStream)
				{
					AddDataBytesToHash(vertexStream->GetDataHash());
				}
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

		ValidateLifetimeCompatibility(m_lifetime, buffer->GetType());

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
}