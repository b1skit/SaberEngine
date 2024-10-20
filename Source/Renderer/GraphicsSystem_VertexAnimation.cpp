// © 2024 Adam Badke. All rights reserved.
#include "EnumTypes.h"
#include "GraphicsSystem_VertexAnimation.h"
#include "GraphicsSystemManager.h"
#include "RenderDataManager.h"

#include "Core/Util/MathUtils.h"


namespace
{
	VertexStreamMetadata GetVertexStreamMetadataData(gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(meshPrimRenderData.m_vertexStreams[0] != nullptr &&
			meshPrimRenderData.m_numVertexStreams > 0,
			"Must have at least 1 vertex stream");

		std::array<gr::VertexStream const*, gr::VertexStream::k_maxVertexStreams> const& vertexStreams = 
			meshPrimRenderData.m_vertexStreams;

		gr::MeshPrimitive::MorphTargetMetadata const& morphMetadata = meshPrimRenderData.m_morphTargetMetadata;

		VertexStreamMetadata streamData{};

		// .x = No. vertices per stream, .y = max morph targets per stream, .z = interleaved morph float stride, .w = unused
		streamData.g_meshPrimMetadata = glm::uvec4( 
			vertexStreams[0]->GetNumElements(),
			meshPrimRenderData.m_morphTargetMetadata.m_maxMorphTargets,
			meshPrimRenderData.m_morphTargetMetadata.m_morphByteStride / sizeof(float),
			0);

		uint8_t streamIdx = 0;
		for (uint8_t i = 0; i < gr::VertexStream::k_maxVertexStreams; ++i)
		{
			if (vertexStreams[i] == nullptr)
			{
				SEAssert(morphMetadata.m_perStreamMetadata[i].m_firstByteOffset == 0 &&
					morphMetadata.m_perStreamMetadata[i].m_byteStride == 0 &&
					morphMetadata.m_perStreamMetadata[i].m_numComponents == 0,
					"Vertex stream is null, but morph metadata is non-zero. This is unexpected");
				break;
			}
			SEAssert(vertexStreams[i]->GetNumElements() == vertexStreams[0]->GetNumElements(),
				"Found a mismatched number of vertexes between streams. This is unexpected");

			SEAssert(vertexStreams[i]->GetDataType() == re::DataType::Float ||
				vertexStreams[i]->GetDataType() == re::DataType::Float2 || 
				vertexStreams[i]->GetDataType() == re::DataType::Float3 || 
				vertexStreams[i]->GetDataType() == re::DataType::Float4,
				"Currently expecting all streams to be float types");

			if (meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[i].m_byteStride == 0 ||
				meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[i].m_numComponents == 0)
			{
				SEAssert(meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[i].m_byteStride == 0 &&
					meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[i].m_numComponents == 0,
					"Byte stride and number of components out of sync: Must be mutally zero/non-zero");

				continue;
			}

			// .x = vertex float stride, .y = no. components, .zw = unused
			streamData.g_streamMetadata[streamIdx] = glm::uvec4(
				vertexStreams[streamIdx]->GetTotalDataByteSize() / (vertexStreams[streamIdx]->GetNumElements() * sizeof(float)),
				DataTypeToNumComponents(vertexStreams[streamIdx]->GetDataType()),
				0,
				0);

			// .x = first float offset, .y = float stride (of 1 displacement), .z = no. components, .w = unused
			streamData.g_morphMetadata[streamIdx] = glm::uvec4(
				morphMetadata.m_perStreamMetadata[streamIdx].m_firstByteOffset / sizeof(float),
				morphMetadata.m_perStreamMetadata[streamIdx].m_byteStride / sizeof(float),
				morphMetadata.m_perStreamMetadata[streamIdx].m_numComponents,
				0);

			++streamIdx;
		}

		return streamData;
	}


	DispatchMetadata GetDispatchMetadataData(uint8_t numStreamBuffers)
	{
		return DispatchMetadata{
			.g_dispatchMetadata = glm::uvec4(numStreamBuffers, 0, 0, 0), // .x = num active buffers, .yzw = unused
		};
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

					// Create/update the morph target weights buffer:
					std::vector<float> const& morphWeights = meshRenderData.m_morphTargetWeights;
					
					auto& buffer = m_meshIDToMeshRenderParams.at(meshRenderDataID);
					if (buffer == nullptr)
					{
						buffer = re::Buffer::CreateArray(
							std::format("MeshRenderData {} Morph Weights", meshRenderDataID),
							morphWeights.data(),
							re::Buffer::BufferParams{
								.m_stagingPool = re::Buffer::StagingPool::Permanent,
								.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
								.m_accessMask = re::Buffer::Access::CPUWrite | re::Buffer::Access::GPURead,
								.m_usageMask = re::Buffer::Usage::Structured,
								.m_arraySize = util::CheckedCast<uint32_t>(morphWeights.size()),
							});
					}
					else
					{
						buffer->Commit(morphWeights.data(), 0, util::CheckedCast<uint32_t>(morphWeights.size()));
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
						SEAssert(m_meshPrimIDToBuffers.contains(visibleID),
							"Failed to find a destination vertex buffer to write to. This should not be possible");

						const uint32_t numVerts = meshPrimRenderData.m_vertexStreams[0]->GetNumElements();
						SEAssert(numVerts >= 3, "Less than 3 verts. This is unexpected");
						
						// We process verts in 1D (round up to ensure we dispatch at least one thread group)
						const uint32_t roundedXDim =
							(numVerts / VERTEX_ANIM_THREADS_X) + (numVerts % VERTEX_ANIM_THREADS_X) == 0 ? 0 : 1;

						AnimationBuffers const& animBuffers = m_meshPrimIDToBuffers.at(visibleID);

						// Process our streams in blocks of max 8 (OpenGL only guarantees 8 SSBO's are accessible at once)
						const uint8_t numDispatches = 
							std::max(animBuffers.m_numAnimatedStreams / MAX_STREAMS_PER_DISPATCH, 1);

						for (uint8_t dispatchIdx = 0; dispatchIdx < numDispatches; ++dispatchIdx)
						{
							re::Batch vertAnimationBatch = re::Batch(
								re::Lifetime::SingleFrame,
								re::Batch::ComputeParams{ .m_threadGroupCount = glm::uvec3(roundedXDim, 1u, 1u) },
								effect::Effect::ComputeEffectID("VertexAnimation"));

							// Set the buffers:
							SEAssert(m_meshIDToMeshRenderParams.contains(meshPrimRenderData.m_owningMeshRenderDataID),
								"MeshPrimitive has an owning Mesh ID that hasn't been registered. This shouldn't be possible");

							// AnimationData: Per-Mesh weights
							vertAnimationBatch.SetBuffer("MorphWeights",
								m_meshIDToMeshRenderParams.at(meshPrimRenderData.m_owningMeshRenderDataID));

							// Attach input/output vertex buffers:
							auto const& destBufferArray = m_meshPrimIDToBuffers.at(visibleID);

							// Attach the current subset of streams:
							const uint8_t firstStreamIdx = dispatchIdx * MAX_STREAMS_PER_DISPATCH;
							const uint8_t endStreamIdx = std::min<uint8_t>(
								meshPrimRenderData.m_numVertexStreams, (dispatchIdx + 1) * MAX_STREAMS_PER_DISPATCH);

							uint8_t bufferShaderIdx = 0;
							for (uint8_t srcIdx = firstStreamIdx; srcIdx < endStreamIdx; ++srcIdx)
							{
								SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx] != nullptr,
									"Found a null stream while iterating over the number of streams");	

								if (meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[srcIdx].m_byteStride == 0 ||
									meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[srcIdx].m_numComponents == 0)
								{
									continue;
								}

								// We view our data as arrays of floats:
								const uint32_t numFloatElements =
									meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements() *
									DataTypeToNumComponents(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType());
								constexpr uint32_t k_floatStride = sizeof(float);

								// Set the input vertex stream buffers:
								vertAnimationBatch.SetBuffer(
									"InVertexStreams",
									meshPrimRenderData.m_vertexStreams[srcIdx]->GetBufferSharedPtr(),
									re::BufferView::BufferType{
										.m_firstElement = 0,
										.m_numElements = numFloatElements,
										.m_structuredByteStride = k_floatStride,
										.m_firstDestIdx = bufferShaderIdx,
									});

								// Set the output vertex stream buffers:
								vertAnimationBatch.SetBuffer(
									"OutVertexStreams",
									destBufferArray.m_destBuffers[srcIdx],
									re::BufferView::BufferType{
										.m_firstElement = 0,
										.m_numElements = numFloatElements,
										.m_structuredByteStride = k_floatStride,
										.m_firstDestIdx = bufferShaderIdx,
									});

								++bufferShaderIdx;
							}

							// Set the dispatch metadata:
							vertAnimationBatch.SetBuffer(
								DispatchMetadata::s_shaderName,
								re::Buffer::Create(
									DispatchMetadata::s_shaderName,
									GetDispatchMetadataData(bufferShaderIdx),
									re::Buffer::BufferParams{
										.m_lifetime = re::Lifetime::SingleFrame,
										.m_stagingPool = re::Buffer::StagingPool::Temporary,
										.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
										.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
										.m_usageMask = re::Buffer::Usage::Constant
									}));

							// Set the vertex stream metadata:
							vertAnimationBatch.SetBuffer(
								"VertexStreamMetadataParams",
								m_meshPrimIDToBuffers.at(visibleID).m_streamMetadataBuffer);

							// Set the interleaved morph data:
							vertAnimationBatch.SetBuffer(
								"MorphData",
								meshPrimRenderData.m_interleavedMorphData);

							m_vertexAnimationStage->AddBatch(vertAnimationBatch);
						}
					}

					seenMeshPrimitives.emplace(visibleID);
				}
			}
		}
	}


	void VertexAnimationGraphicsSystem::AddDestVertexBuffers(
		gr::RenderDataID renderDataID, gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(!m_meshPrimIDToBuffers.contains(renderDataID) && !m_outputs.contains(renderDataID),
			"RenderDataID has already been registered. This should not be possible");

		SEAssert(meshPrimRenderData.m_hasMorphTargets, "Mesh primitive does not have morph targets");

		// Insert new entries for our output buffers/data:
		auto destVertexBuffers = m_meshPrimIDToBuffers.emplace(renderDataID, AnimationBuffers{});

		auto newOutputs = m_outputs.emplace(
			renderDataID, std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams>{});

		for (uint8_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; ++streamIdx)
		{
			if (meshPrimRenderData.m_vertexStreams[streamIdx] == nullptr)
			{
				break;
			}

			// If we've got morph data, create an output buffer to write into
			if (meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_byteStride != 0 ||
				meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_numComponents != 0)
			{
				SEAssert(meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_byteStride != 0 &&
					meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_numComponents != 0,
					"Byte stride and number of components out of sync: Both should be mutually zero/non-zero");

				std::string const& destBuffername = std::format("AnimatedVerts: MeshPrim ID {}, stream {}: {}, Hash:{}",
					renderDataID,
					streamIdx,
					gr::VertexStream::TypeToCStr(meshPrimRenderData.m_vertexStreams[streamIdx]->GetType()),
					meshPrimRenderData.m_vertexStreams[streamIdx]->GetDataHash());

				// Create a destination buffer for our animated vertices:
				destVertexBuffers.first->second.m_destBuffers[streamIdx] = re::Buffer::CreateUnstaged(
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
					destVertexBuffers.first->second.m_destBuffers[streamIdx].get());

				destVertexBuffers.first->second.m_numAnimatedStreams++;
			}
			else // Otherwise, just pass through the existing vertex stream:
			{
				newOutputs.first->second[streamIdx] = re::VertexBufferInput(meshPrimRenderData.m_vertexStreams[streamIdx]);
			}
		}

		// Mesh primitive metadata:
		destVertexBuffers.first->second.m_streamMetadataBuffer = re::Buffer::Create(
			std::format("MeshPrimitiveID {} Vertex stream metadata", renderDataID),
			GetVertexStreamMetadataData(meshPrimRenderData),
			re::Buffer::BufferParams{
				.m_lifetime = re::Lifetime::Permanent,
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
				.m_accessMask = re::Buffer::Access::GPURead,
				.m_usageMask = re::Buffer::Usage::Constant,
			});
	}


	void VertexAnimationGraphicsSystem::RemoveDestVertexBuffers(gr::RenderDataID renderDataID)
	{
		SEAssert(m_meshPrimIDToBuffers.contains(renderDataID) && m_outputs.contains(renderDataID),
			"RenderDataID was not registered. This should not be possible");

		m_meshPrimIDToBuffers.erase(renderDataID);
		m_outputs.erase(renderDataID);
	}
}