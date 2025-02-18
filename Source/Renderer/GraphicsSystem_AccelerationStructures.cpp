// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "GraphicsSystem_AccelerationStructures.h"
#include "GraphicsSystemManager.h"


namespace
{
	std::shared_ptr<re::Buffer> Create3x4RowMajorTransformBuffer(
		std::string const& bufferName, std::vector<glm::mat4 const*> const& worldMatrices)
	{
		const re::Buffer::BufferParams bufferParams{
			.m_lifetime = re::Lifetime::Permanent, // Can't use single-frame buffers, as we need to transition the resource state
			.m_stagingPool = re::Buffer::StagingPool::Temporary,
			.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
			.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
			.m_usageMask = re::Buffer::Usage::Structured,
			.m_arraySize = util::CheckedCast<uint32_t>(worldMatrices.size()),
		};

		std::vector<glm::mat3x4> transformsRowMajor;
		transformsRowMajor.resize(worldMatrices.size(), glm::mat3x4(1.f));

		for (size_t i = 0; i < worldMatrices.size(); ++i)
		{
			transformsRowMajor[i] = glm::mat3x4(glm::transpose(*worldMatrices[i]));
		}

		return re::Buffer::CreateArray<glm::mat3x4>(bufferName, transformsRowMajor.data(), bufferParams);
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
		enum class BLASOperation
		{
			Build,
			Update,
		};
		std::unordered_map<gr::RenderDataID, BLASOperation> meshConceptUpdates;


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
						auto updateItr = meshConceptUpdates.find(owningMeshConceptID);
						if (updateItr != meshConceptUpdates.end())
						{
							meshConceptUpdates.erase(updateItr);
						}
					}
					else
					{
						// If we've still got MeshPrimitives associated with the MeshConcept, we'll need to rebuild as
						// only vertex positions can change in a BLAS (not the no. of geometries etc)
						meshConceptUpdates.emplace(owningMeshConceptID, BLASOperation::Build);
					}
				}
			}			
		}
				
		// Update BLAS's for new geoemtry, or geometry with dirty MeshPrimitives, Materials, or Transforms:
		std::vector<gr::RenderDataID> const& dirtyMeshPrimIDs = renderData.GetIDsWithAnyDirtyData<
			gr::MeshPrimitive::RenderData, gr::Material::MaterialInstanceRenderData, gr::Transform::RenderData>();
		if (!dirtyMeshPrimIDs.empty())
		{
			auto dirtyMeshPrimItr = renderData.IDBegin(dirtyMeshPrimIDs);
			auto const& dirtyMeshPrimEndItr = renderData.IDEnd(dirtyMeshPrimIDs);
			while (dirtyMeshPrimItr != dirtyMeshPrimEndItr)
			{
				if (gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveConcept, dirtyMeshPrimItr.GetFeatureBits()))
				{
					const gr::RenderDataID meshPrimID = dirtyMeshPrimItr.GetRenderDataID();

					gr::MeshPrimitive::RenderData const& meshPrimRenderData =
						dirtyMeshPrimItr.Get<gr::MeshPrimitive::RenderData>();

					const gr::RenderDataID owningMeshConceptID = meshPrimRenderData.m_owningMeshRenderDataID;

					SEAssert(owningMeshConceptID != gr::k_invalidRenderDataID,
						"Found a MeshPrimitive not owned by a MeshConcept");

					m_meshConceptToPrimitiveIDs[owningMeshConceptID].emplace(meshPrimID);
					m_meshPrimToMeshConceptID[meshPrimID] = owningMeshConceptID;

					// Record a BLAS update:
					auto meshConceptUpdateItr = meshConceptUpdates.find(owningMeshConceptID);
					if (meshConceptUpdateItr == meshConceptUpdates.end())
					{
						meshConceptUpdateItr =
							meshConceptUpdates.emplace(owningMeshConceptID, BLASOperation::Update).first;
					}

					// If the geometry or opaque-ness have changed, we must rebuild:
					if (renderData.IsDirty<gr::MeshPrimitive::RenderData>(meshPrimID) ||
						renderData.IsDirty<gr::Material::MaterialInstanceRenderData>(meshPrimID))
					{
						meshConceptUpdateItr->second = BLASOperation::Build;
					}
				}
				++dirtyMeshPrimItr;
			}
		}

		// Update BLAS's for animated geometry:
		for (auto const& entry : *m_animatedVertexStreams)
		{
			SEAssert(m_meshPrimToMeshConceptID.contains(entry.first),
				"Found an animated stream that isn't being tracked. This should not be possible");

			const gr::RenderDataID owningMeshConceptID = m_meshPrimToMeshConceptID.at(entry.first);
			
			// Record a BLAS update:
			auto meshConceptUpdateItr = meshConceptUpdates.find(owningMeshConceptID);
			if (meshConceptUpdateItr == meshConceptUpdates.end())
			{
				meshConceptUpdateItr =
					meshConceptUpdates.emplace(owningMeshConceptID, BLASOperation::Update).first;
			}
		}

		// If we're about to create a BLAS, add a single-frame stage to hold the work:
		re::StagePipeline::StagePipelineItr singleFrameBlasCreateStageItr;
		if (!meshConceptUpdates.empty())
		{
			singleFrameBlasCreateStageItr = m_stagePipeline->AppendSingleFrameStage(m_rtParentStageItr,
				re::Stage::CreateSingleFrameRayTracingStage(
					"Build BLAS stage",
					re::Stage::RayTracingStageParams{}));
		}

		// Create BLAS work:
		for (auto const& record : meshConceptUpdates)
		{
			gr::RenderDataID meshConceptID = record.first;

			SEAssert(m_meshConceptToPrimitiveIDs.contains(meshConceptID),
				"Failed to find MeshConcept record. This should not be possible");

			std::unordered_set<gr::RenderDataID> const& meshPrimIDs = m_meshConceptToPrimitiveIDs.at(meshConceptID);

			switch (record.second)
			{
			case BLASOperation::Build:
			{
				std::vector<glm::mat4 const*> blasMatrices;

				auto blasCreateParams = std::make_unique<re::AccelerationStructure::BLASCreateParams>();

				for (gr::RenderDataID meshPrimID : meshPrimIDs)
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
						renderData.GetObjectData<gr::MeshPrimitive::RenderData>(meshPrimID);

					auto& instance = blasCreateParams->m_instances.emplace_back();

					auto animatedStreamsItr = m_animatedVertexStreams->find(meshPrimID);
					if (animatedStreamsItr != m_animatedVertexStreams->end())
					{
						re::Batch::VertexStreamOverride const& streamOverride = animatedStreamsItr->second;

						instance.m_positions = streamOverride[gr::VertexStream::Position].GetStream();
						instance.m_indices = streamOverride[gr::VertexStream::Index].GetStream(); // May be null
					}
					else
					{
						instance.m_positions = meshPrimRenderData.m_vertexStreams[gr::VertexStream::Position];
						instance.m_indices = meshPrimRenderData.m_vertexStreams[gr::VertexStream::Index]; // May be null
					}

					blasMatrices.emplace_back(&renderData.GetTransformDataFromRenderDataID(meshPrimID).g_model);

					gr::Material::MaterialInstanceRenderData const& materialRenderData =
						renderData.GetObjectData<gr::Material::MaterialInstanceRenderData>(meshPrimID);

					instance.m_geometryFlags = materialRenderData.m_alphaMode == gr::Material::AlphaMode::Opaque ?
						re::AccelerationStructure::GeometryFlags::Opaque :
						re::AccelerationStructure::GeometryFlags::None;
				}

				// Create our BLAS AccelerationStructure object:
				blasCreateParams->m_transform = Create3x4RowMajorTransformBuffer(
					std::format("MeshConcept BLAS {} Transforms", meshConceptID), blasMatrices);

				std::shared_ptr<re::AccelerationStructure> newBLAS = re::AccelerationStructure::CreateBLAS(
					std::format("MeshConcept {} BLAS", meshConceptID).c_str(),
					std::move(blasCreateParams));

				m_meshConceptToBLAS[meshConceptID] = newBLAS;

				// Add a single-frame stage to create the BLAS on the GPU:
				const re::Batch::RayTracingParams blasCreateBatchParams{
					.m_operation = re::Batch::RayTracingParams::Operation::BuildAS,
					.m_accelerationStructure = newBLAS,
				};

				(*singleFrameBlasCreateStageItr)->AddBatch(re::Batch(
					re::Lifetime::SingleFrame, 
					blasCreateBatchParams,
					EffectID()));
			}
			break;
			case BLASOperation::Update:
			{
				SEAssertF("TODO: Implement BLAS Updates");
			}
			break;
			default: SEAssertF("Invalid BLASOperation");
			}
		}
	}


	void AccelerationStructuresGraphicsSystem::ShowImGuiWindow()
	{
		ImGui::Text(std::format("BLAS Count: {}", m_meshConceptToBLAS.size()).c_str());
	}
}