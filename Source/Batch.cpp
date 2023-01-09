// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "MeshPrimitive.h"
#include "Material.h"
#include "Shader.h"
#include "ParameterBlock.h"
#include "Mesh.h"

using std::string;
using std::shared_ptr;
using std::make_shared;

namespace
{
	constexpr size_t k_batchParamBlockIDsReserveAmount = 10;
}


namespace re
{
	Batch::Batch(re::MeshPrimitive* meshPrimitive, gr::Material* material, re::Shader* shader)
		: m_batchMeshPrimitive(meshPrimitive)
		, m_batchMaterial(material)
		, m_batchShader(shader)
		, m_batchGeometryMode(GeometryMode::Indexed)
		, m_batchFilterMask(0)
		, m_numInstances(1)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		// Material params:
		if (material)
		{
			m_batchParamBlocks.emplace_back(material->GetParameterBlock());
		}
		
		ComputeDataHash();
	}


	Batch::Batch(std::shared_ptr<gr::Mesh> const mesh, gr::Material* material, re::Shader* shader)
		: Batch(mesh->GetMeshPrimitives()[0].get(), material, shader)
	{
			SEAssert("Currently only support Mesh with a single MeshPrimitve. TODO: Support > 1 MeshPrimitve", 
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
		
		// Material:
		if (m_batchMaterial)
		{
			AddDataBytesToHash(&m_batchMaterial->GetName()[0], m_batchMaterial->GetName().length() * sizeof(char));
		}

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

		// Note: We don't compute hashes for any batch uniforms here; they're appended as they're added to the batch
	}


	void Batch::SetBatchFilterMaskBit(Filter filterBit)
	{
		m_batchFilterMask |= (1 << (uint32_t)filterBit);
	}
}