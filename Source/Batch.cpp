// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "CastUtils.h"
#include "DebugConfiguration.h"
#include "MeshPrimitive.h"
#include "Material.h"
#include "Mesh.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "Shader.h"
#include "Texture.h"
#include "Transform.h"


using std::string;
using std::shared_ptr;
using std::make_shared;

namespace
{
	constexpr size_t k_batchParamBlockIDsReserveAmount = 10;
}


namespace re
{
	std::vector<re::Batch> Batch::BuildBatches(std::vector<std::shared_ptr<gr::Mesh>> const& meshes)
	{
		std::vector<std::pair<Batch, gr::Transform*>> unmergedBatches;
		unmergedBatches.reserve(meshes.size());
		for (shared_ptr<gr::Mesh> mesh : meshes)
		{
			for (shared_ptr<re::MeshPrimitive> const meshPrimitive : mesh->GetMeshPrimitives())
			{
				unmergedBatches.emplace_back(std::pair<re::Batch, gr::Transform*>(
					{
						meshPrimitive.get(),
						meshPrimitive->GetMeshMaterial()
					},
					mesh->GetTransform()));
			}
		}

		// Sort the batches:
		std::sort(
			unmergedBatches.begin(),
			unmergedBatches.end(),
			[](std::pair<Batch, gr::Transform*> const& a, std::pair<Batch, gr::Transform*> const& b)
			-> bool { return (a.first.GetDataHash() > b.first.GetDataHash()); }
		);

		// Assemble a list of merged batches:
		std::vector<re::Batch> mergedBatches;
		mergedBatches.reserve(meshes.size());
		size_t unmergedIdx = 0;
		do
		{
			// Add the first batch in the sequence to our final list:
			mergedBatches.emplace_back(unmergedBatches[unmergedIdx].first);
			const uint64_t curBatchHash = mergedBatches.back().GetDataHash();

			// Find the index of the last batch with a matching hash in the sequence:
			const size_t instanceStartIdx = unmergedIdx++;
			while (unmergedIdx < unmergedBatches.size() &&
				unmergedBatches[unmergedIdx].first.GetDataHash() == curBatchHash)
			{
				unmergedIdx++;
			}

			// Compute and set the number of instances in the batch:
			const uint32_t numInstances = util::CheckedCast<uint32_t, size_t>(unmergedIdx - instanceStartIdx);

			mergedBatches.back().SetInstanceCount(numInstances);

			// Now build the instanced PBs:
			std::vector<gr::Transform*> instanceTransforms;
			instanceTransforms.reserve(numInstances);

			for (size_t instanceOffset = 0; instanceOffset < numInstances; instanceOffset++)
			{
				// Add the Transform to our list
				const size_t srcIdx = instanceStartIdx + instanceOffset;
				instanceTransforms.emplace_back(unmergedBatches[srcIdx].second);
			}

			std::shared_ptr<re::ParameterBlock> instancedMeshParams =
				gr::Mesh::CreateInstancedMeshParamsData(instanceTransforms);
			// TODO: We're currently creating/destroying these parameter blocks each frame. This is expensive. Instead,
			// we should create a pool of PBs, and reuse by re-buffering data each frame

			mergedBatches.back().SetParameterBlock(instancedMeshParams);
		} while (unmergedIdx < unmergedBatches.size());

		return mergedBatches;
	}


	Batch::Batch(re::MeshPrimitive const* meshPrimitive, gr::Material const* materialOverride)
		: m_type(BatchType::Graphics)
		, m_graphicsParams{
			.m_batchMeshPrimitive = meshPrimitive,
			.m_batchGeometryMode = GeometryMode::Indexed,
			.m_numInstances = 1}
		, m_batchShader(nullptr)
		, m_batchFilterMask(0)
	{
		m_batchParamBlocks.reserve(k_batchParamBlockIDsReserveAmount);

		gr::Material const* material = materialOverride ? materialOverride : meshPrimitive->GetMeshMaterial();
		if (material)
		{
			m_batchShader = material->GetShader();

			// Material textures/samplers:
			for (size_t i = 0; i < material->GetTexureSlotDescs().size(); i++)
			{
				if (material->GetTexureSlotDescs()[i].m_texture && material->GetTexureSlotDescs()[i].m_samplerObject)
				{
					AddTextureAndSamplerInput(
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


	Batch::Batch(std::shared_ptr<gr::Mesh const> const mesh, gr::Material const* materialOverride)
		: Batch(mesh->GetMeshPrimitives()[0].get(), materialOverride)
	{
			SEAssert("Currently only support Mesh with a single MeshPrimitive. TODO: Support > 1 MeshPrimitve", 
			mesh->GetMeshPrimitives().size() == 1);
	}


	Batch::Batch(ComputeParams const& computeParams)
		: m_type(BatchType::Compute)
		, m_computeParams(computeParams)
		, m_batchShader(nullptr)
		, m_batchFilterMask(0)
	{
	}


	void Batch::SetInstanceCount(uint32_t numInstances)
	{
		SEAssert("Invalid type", m_type == BatchType::Graphics);

		// Update the batch draw mode to be an indexed type:
		if (numInstances > 1)
		{
			switch (m_graphicsParams.m_batchGeometryMode)
			{
			case GeometryMode::Indexed:
			{
				m_graphicsParams.m_batchGeometryMode = GeometryMode::IndexedInstanced;
			}
			break;
			default:
				break;
			}
		}

		m_graphicsParams.m_numInstances = numInstances;
	}


	void Batch::ComputeDataHash()
	{
		// Batch filter mask bit:
		AddDataBytesToHash(m_batchFilterMask);

		switch (m_type)
		{
		case BatchType::Graphics:
		{
			// MeshPrimitive data:
			SEAssert("Batch must have a valid MeshPrimitive", m_graphicsParams.m_batchMeshPrimitive);
			AddDataBytesToHash(m_graphicsParams.m_batchMeshPrimitive->GetDataHash());
		}
		break;
		case BatchType::Compute:
		{
			AddDataBytesToHash(m_computeParams.m_threadGroupCount);
		}
		break;
		default:
			SEAssertF("Invalid type");
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

		// Note: We don't compute hashes for batch textures/samplers here; they're appended as they're added
	}


	void Batch::SetFilterMaskBit(Filter filterBit)
	{
		m_batchFilterMask |= (1 << (uint32_t)filterBit);
	}


	void Batch::SetParameterBlock(std::shared_ptr<re::ParameterBlock> paramBlock)
	{
		SEAssert("Cannot set a null parameter block", paramBlock != nullptr);

		SEAssert("Graphics batch number of instances does not match number of elements in the parameter block",
			m_type != BatchType::Graphics ||
			paramBlock->GetNumElements() == m_graphicsParams.m_numInstances);

		m_batchParamBlocks.emplace_back(paramBlock);
	}


	void Batch::AddTextureAndSamplerInput(
		std::string const& shaderName, 
		std::shared_ptr<re::Texture> texture, 
		std::shared_ptr<re::Sampler> sampler, 
		uint32_t srcMip /*= re::Texture::k_allMips*/)
	{
		SEAssert("Invalid shader sampler name", !shaderName.empty());
		SEAssert("Invalid texture", texture != nullptr);
		SEAssert("Invalid sampler", sampler != nullptr);

		m_batchTextureSamplerInputs.emplace_back(
			BatchTextureAndSamplerInput{ shaderName, texture, sampler, srcMip });

		// Include textures/samplers in the batch hash:
		AddDataBytesToHash(texture->GetUniqueID());
		AddDataBytesToHash(sampler->GetUniqueID());
	}
}