// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "CastUtils.h"
#include "Assert.h"
#include "Material.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"


namespace
{
	constexpr size_t k_batchParamBlockIDsReserveAmount = 10;
}

namespace re
{
	Batch::Batch(Lifetime lifetime, gr::MeshPrimitive const* meshPrimitive)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
		, m_batchShader(nullptr)
		, m_batchFilterBitmask(0)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_batchTopologyMode = meshPrimitive->GetMeshParams().m_topologyMode,
		};

		std::vector<re::VertexStream const*> const& vertexStreams = meshPrimitive->GetVertexStreams();
		memset(&m_graphicsParams.m_vertexStreams, 0, m_graphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));
		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(vertexStreams.size()); slotIdx++)
		{
			if (vertexStreams[slotIdx])
			{
				SEAssert((m_lifetime == Lifetime::SingleFrame) ||
					(vertexStreams[slotIdx]->GetLifetime() == re::VertexStream::Lifetime::Permanent && m_lifetime == Lifetime::Permanent),
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
		gr::Material::RenderData const* materialRenderData)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
		, m_batchShader(nullptr)
		, m_batchFilterBitmask(0)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		m_graphicsParams = GraphicsParams{
			.m_batchGeometryMode = GeometryMode::IndexedInstanced,
			.m_numInstances = 1,
			.m_batchTopologyMode = meshPrimRenderData.m_meshPrimitiveParams.m_topologyMode,
		};

		// Zero out our vertex streams array:
		memset(&m_graphicsParams.m_vertexStreams, 0, m_graphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));

		for (uint8_t slotIdx = 0; slotIdx < static_cast<uint8_t>(meshPrimRenderData.m_vertexStreams.size()); slotIdx++)
		{
			if (meshPrimRenderData.m_vertexStreams[slotIdx])
			{
				SEAssert((m_lifetime == Lifetime::SingleFrame) ||
					(meshPrimRenderData.m_vertexStreams[slotIdx]->GetLifetime() == re::VertexStream::Lifetime::Permanent && 
						m_lifetime == Lifetime::Permanent),
					"Cannot add a vertex stream with a single frame lifetime to a permanent batch");

				m_graphicsParams.m_vertexStreams[slotIdx] = meshPrimRenderData.m_vertexStreams[slotIdx];
			}
		}
		m_graphicsParams.m_indexStream = meshPrimRenderData.m_indexStream;
		
		// Material textures/samplers:
		if (materialRenderData)
		{
			for (size_t i = 0; i < materialRenderData->m_material->GetTexureSlotDescs().size(); i++)
			{
				if (materialRenderData->m_material->GetTexureSlotDescs()[i].m_texture &&
					materialRenderData->m_material->GetTexureSlotDescs()[i].m_samplerObject)
				{
					AddTextureAndSamplerInput(
						materialRenderData->m_material->GetTexureSlotDescs()[i].m_shaderSamplerName,
						materialRenderData->m_material->GetTexureSlotDescs()[i].m_texture.get(),
						materialRenderData->m_material->GetTexureSlotDescs()[i].m_samplerObject);
				}
			}

			// Material params:
			std::shared_ptr<re::ParameterBlock> materialParams =
				gr::Material::CreateParameterBlock(materialRenderData->m_material);

			SEAssert((m_lifetime == re::Batch::Lifetime::Permanent &&
					(materialParams->GetType() == re::ParameterBlock::PBType::Mutable ||
						materialParams->GetType() == re::ParameterBlock::PBType::Immutable)) ||
				(lifetime == re::Batch::Lifetime::SingleFrame &&
					materialParams->GetType() == re::ParameterBlock::PBType::SingleFrame),
				"Batch and material parameter block lifetimes are incompatible");

			m_batchParamBlocks.emplace_back(materialParams);
		}

		ComputeDataHash();
	}


	Batch::Batch(Lifetime lifetime, gr::Material const* material, GraphicsParams const& graphicsParams)
		: m_lifetime(lifetime)
		, m_type(BatchType::Graphics)
		, m_graphicsParams{}
		, m_batchShader(nullptr)
		, m_batchFilterBitmask(0)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		m_graphicsParams = graphicsParams;

#if defined(_DEBUG)
		for (uint8_t slotIdx = 0; slotIdx < gr::MeshPrimitive::Slot_Count; slotIdx++)
		{
			SEAssert((m_lifetime == Lifetime::SingleFrame) ||
				(m_graphicsParams.m_vertexStreams[slotIdx]->GetLifetime() == re::VertexStream::Lifetime::Permanent && 
					m_lifetime == Lifetime::Permanent),
				"Cannot add a vertex stream with a single frame lifetime to a permanent batch");
		}
#endif

		if (material)
		{
			// Material textures/samplers:
			for (size_t i = 0; i < material->GetTexureSlotDescs().size(); i++)
			{
				if (material->GetTexureSlotDescs()[i].m_texture && material->GetTexureSlotDescs()[i].m_samplerObject)
				{
					AddTextureAndSamplerInput(
						material->GetTexureSlotDescs()[i].m_shaderSamplerName,
						material->GetTexureSlotDescs()[i].m_texture.get(),
						material->GetTexureSlotDescs()[i].m_samplerObject);
				}
			}

			// Material params:
			std::shared_ptr<re::ParameterBlock> materialParams = gr::Material::CreateParameterBlock(material);

			SEAssert((m_lifetime == re::Batch::Lifetime::Permanent &&
					(materialParams->GetType() == re::ParameterBlock::PBType::Mutable||
						materialParams->GetType() == re::ParameterBlock::PBType::Immutable)) ||
				(lifetime == re::Batch::Lifetime::SingleFrame &&
					materialParams->GetType() == re::ParameterBlock::PBType::SingleFrame),
				"Batch and material parameter block lifetimes are incompatible");

			m_batchParamBlocks.emplace_back(materialParams);
		}

		ComputeDataHash();
	}


	Batch::Batch(Lifetime lifetime, ComputeParams const& computeParams)
		: m_lifetime(lifetime)
		, m_type(BatchType::Compute)
		, m_computeParams(computeParams)
		, m_batchShader(nullptr)
		, m_batchFilterBitmask(0)
	{
	}


	void Batch::SetInstanceCount(uint32_t numInstances)
	{
		SEAssert(m_type == BatchType::Graphics, "Invalid type");

		m_graphicsParams.m_numInstances = numInstances;
	}


	void Batch::ComputeDataHash()
	{		
		AddDataBytesToHash(m_batchFilterBitmask);

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
			AddDataBytesToHash(m_batchShader->GetNameID());
		}

		// Note: We must consider parameter blocks added before instancing has been calcualted, as they allow us to
		// differentiate batches that are otherwise identical. We'll use the same, identical PB on the merged instanced 
		// batches later
		for (size_t i = 0; i < m_batchParamBlocks.size(); i++)
		{
			AddDataBytesToHash(m_batchParamBlocks[i]->GetUniqueID());
		}

		// Note: We don't compute hashes for batch textures/samplers here; they're appended as they're added
	}


	void Batch::SetFilterMaskBit(Filter filterBit)
	{
		m_batchFilterBitmask |= (1 << (uint32_t)filterBit);
	}


	void Batch::SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock)
	{
		SEAssert(paramBlock != nullptr, "Cannot set a null parameter block");

		SEAssert(m_type != BatchType::Graphics ||
			paramBlock->GetNumElements() == m_graphicsParams.m_numInstances,
			"Graphics batch number of instances does not match number of elements in the parameter block");

		m_batchParamBlocks.emplace_back(paramBlock);
	}


	void Batch::AddTextureAndSamplerInput(
		std::string const& shaderName, 
		re::Texture const* texture,
		std::shared_ptr<re::Sampler> sampler, 
		uint32_t srcMip /*= re::Texture::k_allMips*/)
	{
		SEAssert(!shaderName.empty(), "Invalid shader sampler name");
		SEAssert(texture != nullptr, "Invalid texture");
		SEAssert(sampler != nullptr, "Invalid sampler");

		m_batchTextureSamplerInputs.emplace_back(
			BatchTextureAndSamplerInput{ shaderName, texture, sampler, srcMip });

		// Include textures/samplers in the batch hash:
		AddDataBytesToHash(texture->GetUniqueID());
		AddDataBytesToHash(sampler->GetUniqueID());
	}
}