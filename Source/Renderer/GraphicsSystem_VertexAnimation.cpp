// © 2024 Adam Badke. All rights reserved.
#include "AnimationParamsHelpers.h"
#include "EnumTypes.h"
#include "GraphicsSystem_VertexAnimation.h"
#include "GraphicsSystemManager.h"
#include "RenderDataManager.h"

#include "Core/Util/MathUtils.h"


// TODO: Not sure what the final implementation will look like yet. For now, static asserts to keep things in sync
SEStaticAssert(NUM_MORPH_TARGETS == gr::VertexStream::k_maxVertexStreams, "Value is out of sync");
SEStaticAssert(NUM_VERTEX_STREAMS == gr::VertexStream::k_maxVertexStreams, "Value is out of sync");

namespace
{
	VertexStreamMetadata GetVertexStreamMetadataData(
		std::array<gr::VertexStream const*, gr::VertexStream::k_maxVertexStreams> const& vertexStreams)
	{
		SEAssert(vertexStreams[0] != nullptr, "Must have at least 1 vertex stream");

		VertexStreamMetadata streamData{};

		streamData.g_meshPrimMetadata = glm::uvec4( // .x = No. vertices per stream, .yzw = unused
			vertexStreams[0]->GetNumElements(),
			0,
			0,
			0);

		uint8_t numStreams = 0;
		for (uint8_t i = 0; i < gr::VertexStream::k_maxVertexStreams; ++i)
		{
			if (vertexStreams[i] == nullptr)
			{
				break;
			}
			SEAssert(vertexStreams[i]->GetNumElements() == vertexStreams[0]->GetNumElements(),
				"Found a mismatched number of vertexes between streams. This is unexpected");

			SEAssert(vertexStreams[i]->GetDataType() == re::DataType::Float ||
				vertexStreams[i]->GetDataType() == re::DataType::Float2 || 
				vertexStreams[i]->GetDataType() == re::DataType::Float3 || 
				vertexStreams[i]->GetDataType() == re::DataType::Float4,
				"Currently expecting all streams to be float types");

			streamData.g_perStreamMetadata[i] = glm::uvec4(
				DataTypeToNumComponents(vertexStreams[i]->GetDataType()),
				0,
				0,
				0);

			++numStreams;
		}

		return streamData;
	}
}

namespace gr
{
	VertexAnimationGraphicsSystem::VertexAnimationGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void VertexAnimationGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const&,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_viewCullingResults = GetDataDependency<ViewCullingResults>(k_cullingDataInput, dataDependencies);
		SEAssert(m_viewCullingResults, "View culling results cannot (currently) be null");

		m_vertexAnimationStage = 
			re::RenderStage::CreateComputeStage("Vertex Animation Stage", re::RenderStage::ComputeStageParams{});



		pipeline.AppendRenderStage(m_vertexAnimationStage);
	}


	void VertexAnimationGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_cullingDataInput);
	}


	void VertexAnimationGraphicsSystem::RegisterOutputs()
	{
		RegisterDataOutput(k_animatedVertexStreamsOutput, &m_outputs);
	}


	void VertexAnimationGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Remove deleted MeshRenderData:
		std::vector<gr::RenderDataID> const* deletedMeshRenderDataIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::MeshRenderData>();
		if (deletedMeshRenderDataIDs)
		{
			for (gr::RenderDataID renderDataIDToDelete : *deletedMeshRenderDataIDs)
			{
				SEAssert(m_meshIDToMeshRenderParams.contains(renderDataIDToDelete),
					"MeshRenderData not found. This should not be possible");

				m_meshIDToMeshRenderParams.erase(renderDataIDToDelete);
			}
		}

		// Add new buffers for newly added MeshRenderData:
		std::vector<gr::RenderDataID> const* newMeshRenderDataIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::MeshRenderData>();
		if (newMeshRenderDataIDs)
		{
			for (gr::RenderDataID newRenderDataID : *newMeshRenderDataIDs)
			{
				SEAssert(!m_meshIDToMeshRenderParams.contains(newRenderDataID),
					"MeshRenderData already inserted. This should not be possible");

				m_meshIDToMeshRenderParams.emplace(newRenderDataID, nullptr); // We'll populate this later
			}
		}

		// Update buffers for dirty MeshRenderData:
		if (renderData.HasObjectData<gr::MeshPrimitive::MeshRenderData>())
		{
			std::vector<gr::RenderDataID> const* dirtyMeshRenderData =
				renderData.GetIDsWithDirtyData<gr::MeshPrimitive::MeshRenderData>();
			if (dirtyMeshRenderData)
			{
				auto dirtyMeshRenderDataItr = renderData.IDBegin(*dirtyMeshRenderData);
				auto const& dirtyMeshRenderDataItrEnd = renderData.IDEnd(*dirtyMeshRenderData);
				while (dirtyMeshRenderDataItr != dirtyMeshRenderDataItrEnd)
				{
					const gr::RenderDataID meshRenderDataID = dirtyMeshRenderDataItr.GetRenderDataID();
					SEAssert(m_meshIDToMeshRenderParams.contains(meshRenderDataID),
						"MeshRenderData not found. This should not be possible");

					gr::MeshPrimitive::MeshRenderData const& meshRenderData =
						dirtyMeshRenderDataItr.Get<gr::MeshPrimitive::MeshRenderData>();

					AnimationData const& animationParamsData = GetAnimationParamsData(meshRenderData);

					// Create/update the buffer:
					auto& buffer = m_meshIDToMeshRenderParams.at(meshRenderDataID);
					if (buffer == nullptr)
					{
						buffer = re::Buffer::Create(
							AnimationData::s_shaderName,
							animationParamsData,
							re::Buffer::BufferParams{
								.m_stagingPool = re::Buffer::StagingPool::Permanent,
								.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
								.m_accessMask = re::Buffer::Access::CPUWrite | re::Buffer::Access::GPURead,
								.m_usageMask = re::Buffer::Usage::Constant,
							});
					}
					else
					{
						buffer->Commit(animationParamsData);
					}

					++dirtyMeshRenderDataItr;
				}
			}
		}


		// Remove Buffers/VertexBufferInputs for deleted MeshPrimitive RenderDataIDs:
		std::vector<gr::RenderDataID> const* deletedMeshPrimitiveRenderDataIDs = 
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		if (deletedMeshPrimitiveRenderDataIDs)
		{
			auto deletedMeshPrimItr = renderData.IDBegin(*deletedMeshPrimitiveRenderDataIDs);
			auto const& deletedMeshPrimItrEnd = renderData.IDEnd(*deletedMeshPrimitiveRenderDataIDs);
			while (deletedMeshPrimItr != deletedMeshPrimItrEnd)
			{
				gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
					deletedMeshPrimItr.Get<gr::MeshPrimitive::RenderData>();

				if (meshPrimRenderData.m_hasMorphTargets)
				{
					RemoveDestVertexBuffers(deletedMeshPrimItr.GetRenderDataID());
				}
				++deletedMeshPrimItr;
			}
		}


		// Create Buffers/VertexBufferInputs for new MeshPrimitive RenderDataIDs:
		std::vector<gr::RenderDataID> const* newMeshPrimitiveRenderDataIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::RenderData>();
		if (newMeshPrimitiveRenderDataIDs)
		{
			for (gr::RenderDataID newRenderDataID : *newMeshPrimitiveRenderDataIDs)
			{
				gr::MeshPrimitive::RenderData const& meshPrimRenderData =
					renderData.GetObjectData<gr::MeshPrimitive::RenderData>(newRenderDataID);

				if (meshPrimRenderData.m_hasMorphTargets)
				{
					AddDestVertexBuffers(newRenderDataID, meshPrimRenderData);
				}
			}
		}


		// Dispatch compute batches to animate anything that passed culling:
		std::unordered_set<gr::RenderDataID> seenMeshPrimitives;
		for (auto const& viewAndCulledIDs : *m_viewCullingResults)
		{
			for (gr::RenderDataID visibleID : viewAndCulledIDs.second)
			{
				if (!seenMeshPrimitives.contains(visibleID) && 
					renderData.HasObjectData<gr::MeshPrimitive::RenderData>(visibleID))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
						renderData.GetObjectData<gr::MeshPrimitive::RenderData>(visibleID);

					// Dispatch a compute batch:
					if (meshPrimRenderData.m_hasMorphTargets)
					{
						const uint32_t numVerts = meshPrimRenderData.m_vertexStreams[0]->GetNumElements();
						SEAssert(numVerts >= 3, "Less than 3 verts. This is unexpected");

						// We process verts in 1D:
						// - Each CS thread processes all vertex attributes (pos/nml/etc) at the same vertex index
						// - Our CShader declares thread groups are grids of [numthreads(VERTEX_ANIM_THREADS_X, 1, 1)]
						// Thus, we need to dispatch (numVerts / VERTEX_ANIM_THREADS_X) executions (rounded up to ensure
						// we dispatch at least one thread group)
						const uint32_t roundedXDim = 
							(numVerts / VERTEX_ANIM_THREADS_X) + (numVerts % VERTEX_ANIM_THREADS_X) == 0 ? 0 : 1;

						re::Batch vertAnimationBatch = re::Batch(
							re::Lifetime::SingleFrame,
							re::Batch::ComputeParams{ .m_threadGroupCount = glm::uvec3(roundedXDim, 1u, 1u) },
							effect::Effect::ComputeEffectID("VertexAnimation"));

						// Set the buffers:
						SEAssert(m_meshIDToMeshRenderParams.contains(meshPrimRenderData.m_owningMeshRenderDataID),
							"MeshPrimitive has an owning Mesh ID that hasn't been registered. This shouldn't be possible");

						// AnimationData: Per-Mesh weights
						vertAnimationBatch.SetBuffer(AnimationData::s_shaderName,
							m_meshIDToMeshRenderParams.at(meshPrimRenderData.m_owningMeshRenderDataID));

						// Attach input/output vertex buffers:
						SEAssert(m_meshPrimIDToDestBuffers.contains(visibleID),
							"Failed to find a destination vertex buffer to write to. This should not be possible");

						auto const& destBufferArray = m_meshPrimIDToDestBuffers.at(visibleID);
						for (uint8_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; ++streamIdx)
						{
							if (meshPrimRenderData.m_vertexStreams[streamIdx] == nullptr)
							{
								break;
							}
							
							// We view our data as arrays of floats:
							const uint32_t numFloatElements = 
								meshPrimRenderData.m_vertexStreams[streamIdx]->GetNumElements() * 
								DataTypeToNumComponents(meshPrimRenderData.m_vertexStreams[streamIdx]->GetDataType());
							constexpr uint32_t k_floatStride = sizeof(float);

							// Set the input vertex stream buffers:
							vertAnimationBatch.SetBuffer(
								"InVertexStreams",
								meshPrimRenderData.m_vertexStreams[streamIdx]->GetBufferSharedPtr(),
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numFloatElements,
									.m_structuredByteStride = k_floatStride,
									.m_firstDestIdx = streamIdx,
								});

							// Set the output vertex stream buffers:
							vertAnimationBatch.SetBuffer(
								"OutVertexStreams",
								destBufferArray[streamIdx],
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numFloatElements,
									.m_structuredByteStride = k_floatStride,
									.m_firstDestIdx = streamIdx,
								});
						}

						SEAssert(m_meshPrimIDToStreamMetadataBuffer.contains(visibleID),
							"Failed to find a destination vertex buffer to write to. This should not be possible");

						vertAnimationBatch.SetBuffer(
							"VertexStreamMetadataParams",
							m_meshPrimIDToStreamMetadataBuffer.at(visibleID));
						

						m_vertexAnimationStage->AddBatch(vertAnimationBatch);
					}

					seenMeshPrimitives.emplace(visibleID);
				}
			}
		}
	}


	void VertexAnimationGraphicsSystem::AddDestVertexBuffers(
		gr::RenderDataID renderDataID, gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(!m_meshPrimIDToDestBuffers.contains(renderDataID) && !m_outputs.contains(renderDataID),
			"RenderDataID has already been registered. This should not be possible");

		SEAssert(meshPrimRenderData.m_hasMorphTargets, "Mesh primitive does not have morph targets");

		// Insert new entries for our output buffers/data:
		auto destVertexBuffers = m_meshPrimIDToDestBuffers.emplace(
			renderDataID, std::array<std::shared_ptr<re::Buffer>, gr::VertexStream::k_maxVertexStreams>());

		auto newOutputs = m_outputs.emplace(
			renderDataID, std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>{});

		for (uint8_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; ++streamIdx)
		{
			if (meshPrimRenderData.m_vertexStreams[streamIdx] == nullptr)
			{
				break;
			}

			std::string const& destBuffername = std::format("MeshPrimitive RenderDataID {}, stream {}: {}, Hash:{}", 
				renderDataID,
				streamIdx,
				gr::VertexStream::TypeToCStr(meshPrimRenderData.m_vertexStreams[streamIdx]->GetType()),
				meshPrimRenderData.m_vertexStreams[streamIdx]->GetDataHash());
			
			// Create a destination buffer for our animated vertices:
			destVertexBuffers.first->second[streamIdx] = re::Buffer::CreateUnstaged(
				destBuffername,
				meshPrimRenderData.m_vertexStreams[streamIdx]->GetTotalDataByteSize(),
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::None,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
					.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::GPUWrite,
					.m_usageMask = re::Buffer::Structured | re::Buffer::Usage::VertexStream,
				});

			// Create a stream view matching the configuration of the VertexStream, but with our new buffer
			newOutputs.first->second[streamIdx] = re::VertexBufferInput(
				meshPrimRenderData.m_vertexStreams[streamIdx],
				destVertexBuffers.first->second[streamIdx].get());
		}

		// Mesh primitive metadata:
		m_meshPrimIDToStreamMetadataBuffer.emplace(renderDataID,
			re::Buffer::Create(
				std::format("MeshPrimitiveID {} Vertex stream metadata", renderDataID),
				GetVertexStreamMetadataData(meshPrimRenderData.m_vertexStreams),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
					.m_accessMask = re::Buffer::Access::CPUWrite | re::Buffer::Access::GPURead,
					.m_usageMask = re::Buffer::Usage::Constant,
				}));
	}


	void VertexAnimationGraphicsSystem::RemoveDestVertexBuffers(gr::RenderDataID renderDataID)
	{
		SEAssert(m_meshPrimIDToDestBuffers.contains(renderDataID) && m_outputs.contains(renderDataID),
			"RenderDataID was not registered. This should not be possible");

		m_meshPrimIDToDestBuffers.erase(renderDataID);
		m_meshPrimIDToStreamMetadataBuffer.erase(renderDataID);
		m_outputs.erase(renderDataID);
	}
}