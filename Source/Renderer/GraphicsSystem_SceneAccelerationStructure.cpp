// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Batch.h"
#include "BatchBuilder.h"
#include "Buffer.h"
#include "EnumTypes.h"
#include "GraphicsSystem_SceneAccelerationStructure.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "Material.h"
#include "MeshPrimitive.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"
#include "RenderPipeline.h"
#include "Stage.h"
#include "TransformRenderData.h"
#include "VertexStream.h"

#include "Core/Assert.h"

#include "Core/Interfaces/INamedObject.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/HashUtils.h"
#include "Core/Util/HashKey.h"

#include "Renderer/Shaders/Common/RayTracingParams.h"


namespace
{
	void CreateUpdate3x4RowMajorTransformBuffer(
		gr::RenderDataID owningRenderDataID,
		std::shared_ptr<re::Buffer>& transformBuffer,
		std::vector<glm::mat4 const*> const& worldMatrices)
	{
		std::vector<glm::mat3x4> transformsRowMajor;
		transformsRowMajor.resize(worldMatrices.size(), glm::mat3x4(1.f));

		for (size_t i = 0; i < worldMatrices.size(); ++i)
		{
			transformsRowMajor[i] = glm::mat3x4(glm::transpose(*worldMatrices[i]));
		}

		// Create/re-create the Transform buffer:
		if (transformBuffer == nullptr || transformBuffer->GetArraySize() != worldMatrices.size())
		{
			const re::Buffer::BufferParams bufferParams{
				.m_lifetime = re::Lifetime::Permanent, // Can't use single-frame buffers, as we need to transition the resource state
				.m_stagingPool = re::Buffer::StagingPool::Permanent,
				.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
				.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
				.m_usageMask = re::Buffer::Usage::Raw,
				.m_arraySize = util::CheckedCast<uint32_t>(worldMatrices.size()),
			};

			transformBuffer = re::Buffer::CreateArray<glm::mat3x4>(
				std::format("Mesh RenderDataID {} BLAS Transforms", owningRenderDataID), transformsRowMajor.data(), bufferParams);
		}
		else // Update the transform buffer:
		{
			transformBuffer->Commit(transformsRowMajor.data(), 0, util::CheckedCast<uint32_t>(worldMatrices.size()));
		}
	}


	util::HashKey CreateBLASKey(
		gr::RenderDataID owningMeshConceptID, re::AccelerationStructure::InclusionMask inclusionMask)
	{
		util::HashKey result;
		util::AddDataToHash(result, owningMeshConceptID);
		util::AddDataToHash(result, inclusionMask);
		return result;
	}
}

namespace gr
{
	SceneAccelerationStructureGraphicsSystem::SceneAccelerationStructureGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	SceneAccelerationStructureGraphicsSystem::~SceneAccelerationStructureGraphicsSystem()
	{
		if (m_sceneTLAS)
		{
			m_sceneTLAS->Destroy();
		}
		for (auto& entry : m_meshConceptToBLASAndCount)
		{
			for (auto& blas : entry.second)
			{
				blas.second.first->Destroy();
			}
		}
	}


	void SceneAccelerationStructureGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const&, 
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;
		m_rtParentStageItr = pipeline.AppendStage(re::Stage::CreateParentStage("Ray Tracing parent stage"));

		m_animatedVertexStreams =
			GetDataDependency<AnimatedVertexStreams>(k_animatedVertexStreamsInput, dataDependencies);
		SEAssert(m_animatedVertexStreams, "Animated vertex streams map cannot (currently) be null");
	}


	void SceneAccelerationStructureGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_animatedVertexStreamsInput);
	}


	void SceneAccelerationStructureGraphicsSystem::RegisterOutputs()
	{
		RegisterDataOutput(k_sceneTLASOutput, &m_sceneTLAS);
	}


	void SceneAccelerationStructureGraphicsSystem::PreRender()
	{
		// Update the acceleration structure, if required:
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();


		// Build a list of all BLAS's we need to create/recreate.
		// Note: We pack all MeshPrimitives owned by a single MeshConcept into the same BLAS
		std::unordered_map<gr::RenderDataID, re::Batch::RayTracingParams::Operation> meshConceptIDToBatchOp;

		bool mustRebuildTLAS = false;

		// Process any deleted MeshPrimitives:
		std::vector<gr::RenderDataID> const* deletedMeshPrimIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		if (deletedMeshPrimIDs)
		{
			for (gr::RenderDataID deletedPrimitiveID : *deletedMeshPrimIDs)
			{
				// Need to check if we've seen this ID before as we've lost all information about the deleted object
				auto meshPrimToConceptItr = m_meshPrimToMeshConceptID.find(deletedPrimitiveID);
				if (meshPrimToConceptItr != m_meshPrimToMeshConceptID.end())
				{
					const gr::RenderDataID owningMeshConceptID = meshPrimToConceptItr->second;

					// Erase the MeshPrimitive -> MeshConcept record:
					m_meshPrimToMeshConceptID.erase(meshPrimToConceptItr);

					SEAssert(m_meshPrimToBLASKey.contains(deletedPrimitiveID),
						"Failed to find the MeshPrimtiveID. This should not be possible");

					const util::HashKey blasKey = m_meshPrimToBLASKey.at(deletedPrimitiveID);

					SEAssert(m_meshConceptToPrimitiveIDs.contains(owningMeshConceptID) &&
						m_meshConceptToBLASAndCount.contains(owningMeshConceptID) &&
						m_meshConceptToBLASAndCount.at(owningMeshConceptID).contains(blasKey),
						"Failed to find the owning MeshConcept entries. This should not be possible");

					// Erase the MeshPrimitive -> BLAS and BLAS key records:
					auto& blasAndCountMap = m_meshConceptToBLASAndCount.at(owningMeshConceptID);
					if (--blasAndCountMap.at(blasKey).second == 0)
					{
						blasAndCountMap.erase(blasKey);
					}
					m_meshPrimToBLASKey.erase(deletedPrimitiveID);

					// Erase the MeshConcept -> MeshPrimitive record:
					auto primitiveIDs = m_meshConceptToPrimitiveIDs.find(owningMeshConceptID);
					primitiveIDs->second.erase(deletedPrimitiveID);
					if (primitiveIDs->second.empty())
					{
						SEAssert(m_meshConceptToBLASAndCount.at(owningMeshConceptID).empty(),
							"Trying to delete a MeshConcept record that still has a BLAS");

						m_meshConceptToBLASAndCount.erase(owningMeshConceptID);

						// If the MeshConcept record doesn't contain any more MeshPrimitive IDs, erase it
						m_meshConceptToPrimitiveIDs.erase(primitiveIDs);

						// If we previously recorded a Build operation, remove it
						auto updateItr = meshConceptIDToBatchOp.find(owningMeshConceptID);
						if (updateItr != meshConceptIDToBatchOp.end())
						{
							meshConceptIDToBatchOp.erase(updateItr);
						}
					}
					else
					{
						// If we've still got MeshPrimitives associated with the MeshConcept, we'll need to rebuild as
						// only vertex positions can change in a BLAS (not the no. of geometries etc)
						meshConceptIDToBatchOp.emplace(owningMeshConceptID, re::Batch::RayTracingParams::Operation::BuildAS);
					}

					// If we've removed geometry, we must rebuild the TLAS
					mustRebuildTLAS = true;
				}
			}
		}

		// Update BLAS's for new geoemtry, or geometry with dirty MeshPrimitives, Materials, or Transforms:
		for (auto const& meshPrimItr : gr::ObjectAdapter<
			gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceRenderData>(renderData))
		{
			if (meshPrimItr->AnyDirty() == false)
			{
				continue;
			}

			const gr::RenderDataID meshPrimID = meshPrimItr->GetRenderDataID();

			gr::MeshPrimitive::RenderData const& meshPrimRenderData =
				meshPrimItr->Get<gr::MeshPrimitive::RenderData>();

			const gr::RenderDataID owningMeshConceptID = meshPrimRenderData.m_owningMeshRenderDataID;

			SEAssert(owningMeshConceptID != gr::k_invalidRenderDataID,
				"Found a MeshPrimitive not owned by a MeshConcept");

			m_meshConceptToPrimitiveIDs[owningMeshConceptID].emplace(meshPrimID);
			m_meshPrimToMeshConceptID[meshPrimID] = owningMeshConceptID;

			// Create a BLAS key: This uniquely identifies a BLAS based on its owning MeshConcept and material
			// properties that affect the BLAS behavior
			const util::HashKey blasKey = CreateBLASKey(
				owningMeshConceptID, 
				static_cast<re::AccelerationStructure::InclusionMask>(
					gr::Material::MaterialInstanceRenderData::CreateInstanceInclusionMask(
						&meshPrimItr->Get<gr::Material::MaterialInstanceRenderData>())));

			// Create/update the BLAS count:
			bool isNewBlasKey = false;
			auto meshPrimToBLASKeyItr = m_meshPrimToBLASKey.find(meshPrimID);
			if (meshPrimToBLASKeyItr == m_meshPrimToBLASKey.end())
			{
				m_meshPrimToBLASKey.emplace(meshPrimID, blasKey);

				// A brand new MeshPrimitive: Increment the BLAS reference counter
				m_meshConceptToBLASAndCount[owningMeshConceptID][blasKey].second++;

				isNewBlasKey = true;
			}
			else if (meshPrimToBLASKeyItr->second != blasKey)
			{
				// Get the old BLAS key:
				const util::HashKey prevBlasKey = meshPrimToBLASKeyItr->second;

				// Update the meshPrim RenderDataID -> BLAS key map with the new blas key
				meshPrimToBLASKeyItr->second = blasKey; 

				SEAssert(m_meshConceptToBLASAndCount.contains(owningMeshConceptID),
					"Mesh concept ID not found");

				SEAssert(m_meshConceptToBLASAndCount.at(owningMeshConceptID).contains(prevBlasKey),
					"BLAS and count map does not contain the previous BLAS key");

				// Decrement the BLAS reference counter, and erase the record if the count is 0:
				auto meshConceptToBlasCountItr = m_meshConceptToBLASAndCount.at(owningMeshConceptID).find(prevBlasKey);

				SEAssert(meshConceptToBlasCountItr->second.second > 0, "BLAS count about to go out of range");
				if (--meshConceptToBlasCountItr->second.second == 0)
				{
					m_meshConceptToBLASAndCount.at(owningMeshConceptID).erase(prevBlasKey);
				}

				// Add a new BLAS reference:
				m_meshConceptToBLASAndCount[owningMeshConceptID][blasKey].second++;

				isNewBlasKey = true;
			}

			// Record a BLAS update:
			auto meshConceptUpdateItr = meshConceptIDToBatchOp.find(owningMeshConceptID);
			if (meshConceptUpdateItr == meshConceptIDToBatchOp.end())
			{
				meshConceptUpdateItr = meshConceptIDToBatchOp.emplace(
					owningMeshConceptID, re::Batch::RayTracingParams::Operation::UpdateAS).first;
			}

			// If the geometry or opaque-ness have changed, we must rebuild:
			if (meshPrimItr->IsDirty<gr::MeshPrimitive::RenderData>() ||
				isNewBlasKey) // Did material properties affecting the BLAS change?
			{
				meshConceptUpdateItr->second = re::Batch::RayTracingParams::Operation::BuildAS;
				mustRebuildTLAS = true;
			}
		}

		// Update BLAS's for animated geometry:
		for (auto const& entry : *m_animatedVertexStreams)
		{
			SEAssert(m_meshPrimToMeshConceptID.contains(entry.first),
				"Found an animated stream that isn't being tracked. This should not be possible");

			const gr::RenderDataID owningMeshConceptID = m_meshPrimToMeshConceptID.at(entry.first);
			
			// Record a BLAS update:
			if (!meshConceptIDToBatchOp.contains(owningMeshConceptID))
			{
				meshConceptIDToBatchOp.emplace(owningMeshConceptID, re::Batch::RayTracingParams::Operation::UpdateAS);
			}
		}

		// If we're about to build or update an AS, add a single-frame stage to hold the work:
		re::StagePipeline::StagePipelineItr singleFrameBlasCreateStageItr;
		if (!meshConceptIDToBatchOp.empty() || mustRebuildTLAS)
		{
			singleFrameBlasCreateStageItr = m_stagePipeline->AppendSingleFrameStage(m_rtParentStageItr,
				re::Stage::CreateSingleFrameRayTracingStage(
					"Acceleration structure build/update stages",
					re::Stage::RayTracingStageParams{}));
		}

		// Create BLAS work:
		for (auto const& record : meshConceptIDToBatchOp)
		{
			const gr::RenderDataID meshConceptID = record.first;
			const re::Batch::RayTracingParams::Operation batchOperation = record.second;

			SEAssert(m_meshConceptToPrimitiveIDs.contains(meshConceptID),
				"Failed to find MeshConcept record. This should not be possible");

			// Create a BLAS for each group of geometry with the same material properties, to allow accurate filtering
			std::unordered_map<
				re::AccelerationStructure::InclusionMask, 
				std::vector<gr::RenderDataID>> inclusionMaskToRenderDataIDs;
			for (gr::RenderDataID meshPrimID : m_meshConceptToPrimitiveIDs.at(meshConceptID))
			{
				gr::Material::MaterialInstanceRenderData const& materialRenderData =
					renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(meshPrimID);

				const re::AccelerationStructure::InclusionMask inclusionMask = 
					static_cast<re::AccelerationStructure::InclusionMask>(
						gr::Material::MaterialInstanceRenderData::CreateInstanceInclusionMask(&materialRenderData));

				inclusionMaskToRenderDataIDs[inclusionMask].emplace_back(meshPrimID);
			}

			// Build a BLAS for each group of geometry with the same Material flags:
			for (auto const& entry : inclusionMaskToRenderDataIDs)
			{
				std::vector<glm::mat4 const*> blasMatrices;
				auto blasParams = std::make_unique<re::AccelerationStructure::BLASParams>();

				util::HashKey const& blasKey = CreateBLASKey(meshConceptID, entry.first);

				gr::TransformID parentTransformID = gr::k_invalidTransformID; // Maps to the identity Transform
				for (gr::RenderDataID meshPrimID : entry.second)
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData =
						renderData.GetObjectData<gr::MeshPrimitive::RenderData>(meshPrimID);

					re::AccelerationStructure::Geometry& instance = blasParams->m_geometry.emplace_back(meshPrimID);

					gr::MeshPrimitive::RenderData::RegisterGeometryResources(meshPrimRenderData, instance);

					// Replace the position buffer if it is animated:
					auto animatedStreamsItr = m_animatedVertexStreams->find(meshPrimID);
					if (animatedStreamsItr != m_animatedVertexStreams->end())
					{
						instance.SetVertexPositions(animatedStreamsItr->second[gr::VertexStream::Position]);
					}

					// We use the MeshPrimitive's local TRS matrix for our BLAS, and then use the parent's global TRS to
					// orient our BLAS in the TLAS
					gr::Transform::RenderData const& meshPrimTransform =
						renderData.GetTransformDataFromRenderDataID(meshPrimID);

					SEAssert(parentTransformID == gr::k_invalidTransformID ||
						parentTransformID == meshPrimTransform.m_parentTransformID,
						"MeshPrimitive does not have the same parent transform ID as the previous iterations");

					parentTransformID = meshPrimTransform.m_parentTransformID;

					blasMatrices.emplace_back(&meshPrimTransform.g_local);

					gr::Material::MaterialInstanceRenderData const& materialRenderData =
						renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(meshPrimID);

					gr::Material::MaterialInstanceRenderData::RegisterGeometryResources(materialRenderData, instance);

					// Map the MeshPrimitive RenderDataID -> BLAS key:
					m_meshPrimToBLASKey[meshPrimID] = blasKey;
				}

				// Set the world Transform for all geometries in the BLAS
				// Note: AS matrices must be 3x4 in row-major order
				blasParams->m_blasWorldMatrix = glm::transpose(
					renderData.GetTransformDataFromTransformID(parentTransformID).g_model);

				// Assume we'll always update and compact for now
				blasParams->m_buildFlags = static_cast<re::AccelerationStructure::BuildFlags>
					(re::AccelerationStructure::BuildFlags::AllowUpdate |
						re::AccelerationStructure::BuildFlags::AllowCompaction);

				blasParams->m_instanceMask = entry.first; // Visiblity mask
				blasParams->m_instanceFlags = re::AccelerationStructure::InstanceFlags_None;

				SEAssert(m_meshConceptToBLASAndCount.contains(meshConceptID) &&
					m_meshConceptToBLASAndCount.at(meshConceptID).contains(blasKey),
					"Could not find an existing BLAS record");

				std::shared_ptr<re::AccelerationStructure> blas;
				if (batchOperation == re::Batch::RayTracingParams::Operation::BuildAS)
				{
					// Create a Transform buffer:
					CreateUpdate3x4RowMajorTransformBuffer(meshConceptID, blasParams->m_transform, blasMatrices);

					blas = re::AccelerationStructure::CreateBLAS(
						std::format("Mesh RenderDataID {} BLAS", meshConceptID).c_str(),
						std::move(blasParams));

					m_meshConceptToBLASAndCount.at(meshConceptID).at(blasKey).first = blas; // Create/replace the BLAS
				}
				else // Updating an existing BLAS:
				{
					blas = m_meshConceptToBLASAndCount.at(meshConceptID).at(blasKey).first;

					// Update the existing Transform buffer
					re::AccelerationStructure::BLASParams const* existingBLASParams =
						dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());

					blasParams->m_transform = existingBLASParams->m_transform;
					CreateUpdate3x4RowMajorTransformBuffer(meshConceptID, blasParams->m_transform, blasMatrices);

					blas->UpdateASParams(std::move(blasParams));
				}

				// Add a single-frame stage to create/update the BLAS on the GPU:
				(*singleFrameBlasCreateStageItr)->AddBatch(gr::RayTraceBatchBuilder()
					.SetOperation(batchOperation)
					.SetASInput(re::ASInput(blas))
					.Build());
			}
		}


		// Rebuild the scene TLAS if necessary (i.e. if anything was modified or animated)
		if (!meshConceptIDToBatchOp.empty() || mustRebuildTLAS)
		{
			// Schedule a single-frame stage to create/update the TLAS on the GPU:
			re::Batch::RayTracingParams::Operation tlasOperation = re::Batch::RayTracingParams::Operation::Invalid;
			if (mustRebuildTLAS)
			{
				tlasOperation = re::Batch::RayTracingParams::Operation::BuildAS;

				auto tlasParams = std::make_unique<re::AccelerationStructure::TLASParams>();

				// Assume we'll always update and compact for now
				tlasParams->m_buildFlags = static_cast<re::AccelerationStructure::BuildFlags>
					(re::AccelerationStructure::BuildFlags::AllowUpdate |
						re::AccelerationStructure::BuildFlags::AllowCompaction);

				// Pack the scene BLAS instances:
				for (auto const& entry : m_meshConceptToBLASAndCount)
				{
					for (auto const& blasInstance : entry.second)
					{
						tlasParams->AddBLASInstance(blasInstance.second.first);
					}
				}

				if (tlasParams->GetBLASCount() > 0)
				{
					// Configure the shader binding table:
					const EffectID rtEffectID = effect::Effect::ComputeEffectID("RayTracing");

					// TODO: Support multiple SBTs per AccelerationStructure
					const re::ShaderBindingTable::SBTParams sbtParams{
						.m_rayGenStyles = {
							{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_RayGen_A},
							{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_RayGen_B},
						},
						.m_missStyles = {
							{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_Miss_Blue},
							{rtEffectID, effect::drawstyle::RT_Experimental_RT_Experimental_Miss_Red},
						},
						.m_hitgroupStyles = effect::drawstyle::RT_Experimental_RT_Experimental_Geometry,
						.m_maxPayloadByteSize = sizeof(HitInfo_Experimental),
						.m_maxRecursionDepth = 2,
					};

					// Create a new AccelerationStructure:
					m_sceneTLAS = re::AccelerationStructure::CreateTLAS("Scene TLAS", std::move(tlasParams), sbtParams);
				}
				else
				{
					m_sceneTLAS = nullptr; // Everything must have been deleted
				}
			}
			else
			{
				tlasOperation = re::Batch::RayTracingParams::Operation::UpdateAS;
			}
			
			if (m_sceneTLAS) // Ensure we don't try and build a null TLAS
			{
				re::Batch::RayTracingParams tlasBatchParams;
				tlasBatchParams.m_operation = tlasOperation,
					tlasBatchParams.m_ASInput = re::ASInput(m_sceneTLAS);

				(*singleFrameBlasCreateStageItr)->AddBatch(gr::RayTraceBatchBuilder()
					.SetOperation(tlasOperation)
					.SetASInput(m_sceneTLAS)
					.Build());
			}
		}
	}


	void SceneAccelerationStructureGraphicsSystem::ShowImGuiWindow()
	{
		size_t numBLASes = 0;
		for (auto const& meshConceptRecord : m_meshConceptToBLASAndCount)
		{
			numBLASes += meshConceptRecord.second.size();
		}
		ImGui::Text(std::format("BLAS Count: {}", numBLASes).c_str());
	}
}