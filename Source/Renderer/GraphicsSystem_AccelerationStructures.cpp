// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "GraphicsSystem_AccelerationStructures.h"
#include "GraphicsSystemManager.h"


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
}

namespace gr
{
	AccelerationStructuresGraphicsSystem::AccelerationStructuresGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	AccelerationStructuresGraphicsSystem::~AccelerationStructuresGraphicsSystem()
	{
		if (m_sceneTLAS)
		{
			m_sceneTLAS->Destroy();
		}
		for (auto& blas : m_meshConceptToBLAS)
		{
			blas.second->Destroy();
		}
	}


	void AccelerationStructuresGraphicsSystem::InitPipeline(
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


	void AccelerationStructuresGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_animatedVertexStreamsInput);
	}


	void AccelerationStructuresGraphicsSystem::RegisterOutputs()
	{
		RegisterDataOutput(k_sceneTLASOutput, &m_sceneTLAS);
	}


	void AccelerationStructuresGraphicsSystem::PreRender()
	{
		// Update the acceleration structure, if required:
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();


		// Build a list of all BLAS's we need to create/recreate.
		// Note: We pack all MeshPrimitives owned by a single MeshConcept into the same BLAS
		std::unordered_map<gr::RenderDataID, re::Batch::RayTracingParams::Operation> meshConceptIDToBatchOp;

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

					SEAssert(m_meshConceptToPrimitiveIDs.contains(owningMeshConceptID) &&
						m_meshConceptToBLAS.contains(owningMeshConceptID),
						"Failed to find the owning MeshConcept entries. This should not be possible");

					// Erase the MeshConcept -> MeshPrimitive record:
					auto meshConceptItr = m_meshConceptToPrimitiveIDs.find(owningMeshConceptID);
					meshConceptItr->second.erase(deletedPrimitiveID);

					// If the MeshConcept record doesn't contain any more MeshPrimitive IDs, erase it
					if (meshConceptItr->second.empty())
					{
						m_meshConceptToPrimitiveIDs.erase(meshConceptItr);
						m_meshConceptToBLAS.erase(owningMeshConceptID);

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
				}
			}			
		}

		// Update BLAS's for new geoemtry, or geometry with dirty MeshPrimitives, Materials, or Transforms:
		auto meshPrimItr = renderData.ObjectBegin<gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceRenderData>(
			gr::RenderObjectFeature::IsMeshPrimitiveConcept);
		auto const& meshPrimEndItr = renderData.ObjectEnd<gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceRenderData>();
		while (meshPrimItr != meshPrimEndItr)
		{
			if (meshPrimItr.AnyDirty() == false)
			{
				++meshPrimItr;
				continue;
			}
			const gr::RenderDataID meshPrimID = meshPrimItr.GetRenderDataID();

			gr::MeshPrimitive::RenderData const& meshPrimRenderData =
				meshPrimItr.Get<gr::MeshPrimitive::RenderData>();

			const gr::RenderDataID owningMeshConceptID = meshPrimRenderData.m_owningMeshRenderDataID;

			SEAssert(owningMeshConceptID != gr::k_invalidRenderDataID,
				"Found a MeshPrimitive not owned by a MeshConcept");

			m_meshConceptToPrimitiveIDs[owningMeshConceptID].emplace(meshPrimID);
			m_meshPrimToMeshConceptID[meshPrimID] = owningMeshConceptID;

			// Record a BLAS update:
			auto meshConceptUpdateItr = meshConceptIDToBatchOp.find(owningMeshConceptID);
			if (meshConceptUpdateItr == meshConceptIDToBatchOp.end())
			{
				meshConceptUpdateItr = meshConceptIDToBatchOp.emplace(
					owningMeshConceptID, re::Batch::RayTracingParams::Operation::UpdateAS).first;
			}

			// If the geometry or opaque-ness have changed, we must rebuild:
			if (meshPrimItr.IsDirty<gr::MeshPrimitive::RenderData>() ||
				meshPrimItr.IsDirty<gr::Material::MaterialInstanceRenderData>())
			{
				meshConceptUpdateItr->second = re::Batch::RayTracingParams::Operation::BuildAS;
			}

			++meshPrimItr;
		}

		// Update BLAS's for animated geometry:
		for (auto const& entry : *m_animatedVertexStreams)
		{
			SEAssert(m_meshPrimToMeshConceptID.contains(entry.first),
				"Found an animated stream that isn't being tracked. This should not be possible");

			const gr::RenderDataID owningMeshConceptID = m_meshPrimToMeshConceptID.at(entry.first);
			
			// Record a BLAS update:
			auto meshConceptUpdateItr = meshConceptIDToBatchOp.find(owningMeshConceptID);
			if (meshConceptUpdateItr == meshConceptIDToBatchOp.end())
			{
				meshConceptUpdateItr = meshConceptIDToBatchOp.emplace(
					owningMeshConceptID, re::Batch::RayTracingParams::Operation::UpdateAS).first;
			}
		}

		// If we're about to create a BLAS, add a single-frame stage to hold the work:
		re::StagePipeline::StagePipelineItr singleFrameBlasCreateStageItr;
		if (!meshConceptIDToBatchOp.empty())
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

			std::unordered_set<gr::RenderDataID> const& meshPrimIDs = m_meshConceptToPrimitiveIDs.at(meshConceptID);

			std::vector<glm::mat4 const*> blasMatrices;
			auto blasParams = std::make_unique<re::AccelerationStructure::BLASParams>();
			
			gr::TransformID parentTransformID = gr::k_invalidTransformID; // Render data maps to the identity Transform
			for (gr::RenderDataID meshPrimID : meshPrimIDs)
			{
				gr::MeshPrimitive::RenderData const& meshPrimRenderData =
					renderData.GetObjectData<gr::MeshPrimitive::RenderData>(meshPrimID);

				auto& instance = blasParams->m_geometry.emplace_back();

				// Get the position buffer: Animated, or static
				auto animatedStreamsItr = m_animatedVertexStreams->find(meshPrimID);
				if (animatedStreamsItr != m_animatedVertexStreams->end())
				{
					re::Batch::VertexStreamOverride const& streamOverride = animatedStreamsItr->second;

					instance.m_positions = streamOverride[gr::VertexStream::Position].GetStream();
				}
				else
				{
					instance.m_positions = meshPrimRenderData.m_vertexStreams[gr::VertexStream::Position];
				}

				// Always the same instance buffer, regardless of animation
				instance.m_indices = meshPrimRenderData.m_indexStream; // May be null

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

				instance.m_geometryFlags = materialRenderData.m_alphaMode == gr::Material::AlphaMode::Opaque ?
					re::AccelerationStructure::GeometryFlags::Opaque :
					re::AccelerationStructure::GeometryFlags::GeometryFlags_None;
			}

			// Set the world Transform for all geometries in the BLAS
			// Note: AS matrices must be 3x4 in row-major order
			blasParams->m_blasWorldMatrix = glm::transpose(
				renderData.GetTransformDataFromTransformID(parentTransformID).g_model);

			// Assume we'll always update and compact for now
			blasParams->m_buildFlags = static_cast<re::AccelerationStructure::BuildFlags>
				(re::AccelerationStructure::BuildFlags::AllowUpdate |
					re::AccelerationStructure::BuildFlags::AllowCompaction);

			blasParams->m_hitGroupIdx = 0; // TODO: Set this correctly
			blasParams->m_instanceMask = 0xFF; // Visiblity mask: Always visible, for now
			blasParams->m_instanceFlags = re::AccelerationStructure::InstanceFlags::InstanceFlags_None;

			std::shared_ptr<re::AccelerationStructure> blas;
			if (batchOperation == re::Batch::RayTracingParams::Operation::BuildAS)
			{
				// Create a Transform buffer:
				CreateUpdate3x4RowMajorTransformBuffer(meshConceptID, blasParams->m_transform, blasMatrices);

				blas = re::AccelerationStructure::CreateBLAS(
					std::format("Mesh RenderDataID {} BLAS", meshConceptID).c_str(),
					std::move(blasParams));

				m_meshConceptToBLAS[meshConceptID] = blas; // Create/replace the BLAS
			}
			else // Updating an existing BLAS:
			{
				blas = m_meshConceptToBLAS.at(meshConceptID);

				// Update the existing Transform buffer, and 
				re::AccelerationStructure::BLASParams const* existingBLASParams = 
					dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());

				blasParams->m_transform = std::move(existingBLASParams->m_transform);
				CreateUpdate3x4RowMajorTransformBuffer(meshConceptID, blasParams->m_transform, blasMatrices);
				
				blas->UpdateASParams(std::move(blasParams));
			}

			// Add a single-frame stage to create/update the BLAS on the GPU:
			const re::Batch::RayTracingParams blasCreateBatchParams{
				.m_operation = batchOperation,
				.m_accelerationStructure = blas,
			};

			(*singleFrameBlasCreateStageItr)->AddBatch(re::Batch(re::Lifetime::SingleFrame, blasCreateBatchParams));
		}


		// Rebuild the scene TLAS if necessary
		if (!meshConceptIDToBatchOp.empty())
		{
			bool isBuildingNewBLAS = false; // We'll update the TLAS, unless a BLAS has been (re)built
			for (auto const& update : meshConceptIDToBatchOp)
			{
				if (update.second == re::Batch::RayTracingParams::Operation::BuildAS)
				{
					isBuildingNewBLAS = true;
					break;
				}
			}		

			// Schedule a single-frame stage to create/update the TLAS on the GPU:
			re::Batch::RayTracingParams::Operation tlasOperation = re::Batch::RayTracingParams::Operation::Invalid;
			if (isBuildingNewBLAS)
			{
				tlasOperation = re::Batch::RayTracingParams::Operation::BuildAS;

				auto tlasParams = std::make_unique<re::AccelerationStructure::TLASParams>();

				// Assume we'll always update and compact for now
				tlasParams->m_buildFlags = static_cast<re::AccelerationStructure::BuildFlags>
					(re::AccelerationStructure::BuildFlags::AllowUpdate |
						re::AccelerationStructure::BuildFlags::AllowCompaction);

				// Pack the scene BLAS instances:
				for (auto const& blasInstance : m_meshConceptToBLAS)
				{
					tlasParams->m_blasInstances.emplace_back(blasInstance.second);
				}

				// Create a new AccelerationStructure:
				m_sceneTLAS = re::AccelerationStructure::CreateTLAS("Scene TLAS", std::move(tlasParams));
			}
			else
			{
				tlasOperation = re::Batch::RayTracingParams::Operation::UpdateAS;
			}
			
			re::Batch::RayTracingParams tlasBatchParams{
				.m_operation = tlasOperation,
				.m_accelerationStructure = m_sceneTLAS,
			};

			(*singleFrameBlasCreateStageItr)->AddBatch(re::Batch(re::Lifetime::SingleFrame, tlasBatchParams));
		}
	}


	void AccelerationStructuresGraphicsSystem::ShowImGuiWindow()
	{
		ImGui::Text(std::format("BLAS Count: {}", m_meshConceptToBLAS.size()).c_str());
	}
}