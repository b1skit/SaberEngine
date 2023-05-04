// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "DebugConfiguration.h"
#include "MeshPrimitive.h"
#include "Material.h"
#include "Mesh.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"


using std::string;
using std::shared_ptr;
using std::make_shared;

namespace
{
	constexpr size_t k_batchParamBlockIDsReserveAmount = 10;
}


namespace re
{
	Batch::Batch(re::MeshPrimitive* meshPrimitive, gr::Material* material)
		: m_batchMeshPrimitive(meshPrimitive)
		, m_batchShader(nullptr)
		, m_batchGeometryMode(GeometryMode::Indexed)
		, m_batchFilterMask(0)
		, m_numInstances(1)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		if (material)
		{
			m_batchShader = material->GetShader();

			// Material textures/samplers:
			for (size_t i = 0; i < material->GetTexureSlotDescs().size(); i++)
			{
				if (material->GetTexureSlotDescs()[i].m_texture && material->GetTexureSlotDescs()[i].m_samplerObject)
				{
					AddBatchTextureAndSamplerInput(
						material->GetTexureSlotDescs()[i].m_shaderSamplerName,
						material->GetTexureSlotDescs()[i].m_texture,
						material->GetTexureSlotDescs()[i].m_samplerObject);
				}				
			}

			// Material params:
			std::shared_ptr<re::ParameterBlock> materialParams = material->GetParameterBlock();
			if (materialParams)
			{
				m_batchParamBlocks.emplace_back(materialParams);
			}			
		}
		
		ComputeDataHash();
	}


	Batch::Batch(std::shared_ptr<gr::Mesh> const mesh, gr::Material* material)
		: Batch(mesh->GetMeshPrimitives()[0].get(), material)
	{
			SEAssert("Currently only support Mesh with a single MeshPrimitive. TODO: Support > 1 MeshPrimitve", 
			mesh->GetMeshPrimitives().size() == 1);
	}


	void Batch::IncrementBatchInstanceCount()
	{
		// Update the batch draw mode to be an indexed type:
		switch (m_batchGeometryMode)
		{
		case GeometryMode::Indexed:
		{
			m_batchGeometryMode = GeometryMode::IndexedInstanced;
		}
		break;
		default:
			break;
		}

		m_numInstances++;
	}


	void Batch::ComputeDataHash()
	{
		// Batch filter mask bit:
		AddDataBytesToHash(m_batchFilterMask);

		// MeshPrimitive data:
		SEAssert("Batch must have a valid MeshPrimitive", m_batchMeshPrimitive);
		AddDataBytesToHash(m_batchMeshPrimitive->GetDataHash());

		// Shader:
		if (m_batchShader)
		{
			AddDataBytesToHash(&m_batchShader->GetName()[0], m_batchShader->GetName().length() * sizeof(char));
		}

		// Parameter blocks:
		for (size_t i = 0; i < m_batchParamBlocks.size(); i++)
		{
			AddDataBytesToHash(m_batchParamBlocks[i]->GetUniqueID());
		}

		// Note: We don't compute hashes for batch textures/samplers here; they're appended as they're added
	}


	void Batch::SetBatchFilterMaskBit(Filter filterBit)
	{
		m_batchFilterMask |= (1 << (uint32_t)filterBit);
	}


	void Batch::AddBatchTextureAndSamplerInput(
		std::string const& shaderName, std::shared_ptr<re::Texture> texture, std::shared_ptr<re::Sampler> sampler)
	{
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", texture != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		m_batchTextureSamplerInputs.emplace_back(shaderName, texture, sampler);

		// Include textures/samplers in the batch hash:
		AddDataBytesToHash(texture->GetUniqueID());
		AddDataBytesToHash(sampler->GetUniqueID());
	}
}