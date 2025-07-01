// © 2024 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "EnumTypes.h"
#include "GraphicsSystem_VertexAnimation.h"
#include "GraphicsSystemManager.h"
#include "RenderDataManager.h"

#include "Core/Util/MathUtils.h"

#include "Renderer/Shaders/Common/AnimationParams.h"


namespace
{
	static const EffectID k_vertexAnimationEffectID = effect::Effect::ComputeEffectID("VertexAnimation");


	MorphMetadata GetMorphMetadataData(gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(meshPrimRenderData.m_vertexStreams[0] != nullptr &&
			meshPrimRenderData.m_numVertexStreams > 0,
			"Must have at least 1 vertex stream");

		std::array<core::InvPtr<re::VertexStream>, re::VertexStream::k_maxVertexStreams> const& vertexStreams =
			meshPrimRenderData.m_vertexStreams;

		gr::MeshPrimitive::MorphTargetMetadata const& morphMetadata = meshPrimRenderData.m_morphTargetMetadata;

		MorphMetadata streamData{};

		// .x = No. vertices per stream, .y = max morph targets per stream, .z = interleaved morph float stride, .w = unused
		streamData.g_meshPrimMetadata = glm::uvec4( 
			vertexStreams[0]->GetNumElements(),
			meshPrimRenderData.m_morphTargetMetadata.m_maxMorphTargets,
			meshPrimRenderData.m_morphTargetMetadata.m_morphByteStride / sizeof(float),
			0);

		uint8_t streamIdx = 0;
		for (uint8_t i = 0; i < re::VertexStream::k_maxVertexStreams; ++i)
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


	MorphDispatchMetadata GetMorphDispatchMetadataData(uint8_t numStreamBuffers)
	{
		return MorphDispatchMetadata{
			.g_dispatchMetadata = glm::uvec4(numStreamBuffers, 0, 0, 0), // .x = num active buffers, .yzw = unused
		};
	}


	SkinningData GetSkinningData(gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(meshPrimRenderData.m_vertexStreams[0] != nullptr &&
			meshPrimRenderData.m_numVertexStreams > 0,
			"Must have at least 1 vertex stream");

		SkinningData skinningData{};

		skinningData.g_meshPrimMetadata = glm::uvec4(
			meshPrimRenderData.m_vertexStreams[0]->GetNumElements(),
			0,
			0,
			0);

		return skinningData;
	}


	void UpdateSkinningJointsBuffer(
		gr::MeshPrimitive::SkinningRenderData const& skinData,
		std::shared_ptr<re::Buffer> const& skinningJointsBuffer)
	{
		std::vector<SkinningJoint> jointData;
		jointData.reserve(skinData.m_jointTransforms.size());

		for (size_t jointIdx = 0; jointIdx < skinData.m_jointTransforms.size(); ++jointIdx)
		{
			SkinningJoint& joint = jointData.emplace_back(SkinningJoint{
				.g_joint = skinData.m_jointTransforms[jointIdx],
				.g_transposeInvJoint = skinData.m_transposeInvJointTransforms[jointIdx], });
		}

		skinningJointsBuffer->Commit(jointData.data(), 0, util::CheckedCast<uint32_t>(jointData.size()));
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
		gr::StagePipeline& pipeline,
		TextureDependencies const&,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_viewCullingResults = GetDataDependency<ViewCullingResults>(k_cullingDataInput, dataDependencies);

		m_morphAnimationStage = 
			gr::Stage::CreateComputeStage("Morph Animation Stage", gr::Stage::ComputeStageParams{});
		m_morphAnimationStage->AddDrawStyleBits(effect::drawstyle::VertexAnimation_Morph);
		
		pipeline.AppendStage(m_morphAnimationStage);

		m_skinAnimationStage =
			gr::Stage::CreateComputeStage("Skinned Animation Stage", gr::Stage::ComputeStageParams{});
		m_skinAnimationStage->AddDrawStyleBits(effect::drawstyle::VertexAnimation_Skinning);

		pipeline.AppendStage(m_skinAnimationStage);
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

		// Remove deleted MeshMorphRenderData:
		std::vector<gr::RenderDataID> const* deletedMorphRenderDataIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::MeshMorphRenderData>();
		if (deletedMorphRenderDataIDs)
		{
			for (gr::RenderDataID renderDataIDToDelete : *deletedMorphRenderDataIDs)
			{
				SEAssert(m_meshIDToMorphWeights.contains(renderDataIDToDelete),
					"MeshMorphRenderData not found. This should not be possible");

				m_meshIDToMorphWeights.erase(renderDataIDToDelete);
			}
		}

		// Add new buffers for newly added MeshMorphRenderData:
		std::vector<gr::RenderDataID> const* newMorphRenderDataIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::MeshMorphRenderData>();
		if (newMorphRenderDataIDs)
		{
			for (gr::RenderDataID newRenderDataID : *newMorphRenderDataIDs)
			{
				SEAssert(!m_meshIDToMorphWeights.contains(newRenderDataID),
					"MeshMorphRenderData already inserted. This should not be possible");

				m_meshIDToMorphWeights.emplace(newRenderDataID, nullptr); // We'll populate this later
			}
		}

		// Update buffers for dirty MeshMorphRenderData:
		if (renderData.HasObjectData<gr::MeshPrimitive::MeshMorphRenderData>())
		{
			std::vector<gr::RenderDataID> const* dirtyMorphRenderData =
				renderData.GetIDsWithDirtyData<gr::MeshPrimitive::MeshMorphRenderData>();
			if (dirtyMorphRenderData)
			{
				for (auto const& dirtyMeshRenderDataItr : gr::IDAdapter(renderData, *dirtyMorphRenderData))
				{
					const gr::RenderDataID meshRenderDataID = dirtyMeshRenderDataItr->GetRenderDataID();
					SEAssert(m_meshIDToMorphWeights.contains(meshRenderDataID),
						"MeshMorphRenderData not found. This should not be possible");

					gr::MeshPrimitive::MeshMorphRenderData const& meshRenderData =
						dirtyMeshRenderDataItr->Get<gr::MeshPrimitive::MeshMorphRenderData>();

					// Create/update the morph target weights buffer:
					std::vector<float> const& morphWeights = meshRenderData.m_morphTargetWeights;
					
					auto& buffer = m_meshIDToMorphWeights.at(meshRenderDataID);
					if (buffer == nullptr)
					{
						buffer = re::Buffer::CreateArray(
							std::format("MeshMorphRenderData {} Morph Weights", meshRenderDataID),
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
				}
			}
		}


		// Remove deleted skinned meshes:
		std::vector<gr::RenderDataID> const* deletedSkinnedMeshRenderDataIDs =
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::SkinningRenderData>();
		if (deletedSkinnedMeshRenderDataIDs)
		{
			for (gr::RenderDataID renderDataIDToDelete : *deletedSkinnedMeshRenderDataIDs)
			{
				SEAssert(m_meshIDToSkinJoints.contains(renderDataIDToDelete),
					"SkinningRenderData not found. This should not be possible");

				m_meshIDToSkinJoints.erase(renderDataIDToDelete);
			}
		}

		// Add new skinned meshes:
		std::vector<gr::RenderDataID> const* newSkinnedMeshRenderDataIDs =
			renderData.GetIDsWithNewData<gr::MeshPrimitive::SkinningRenderData>();
		if (newSkinnedMeshRenderDataIDs)
		{
			for (gr::RenderDataID newRenderDataID : *newSkinnedMeshRenderDataIDs)
			{
				SEAssert(!m_meshIDToSkinJoints.contains(newRenderDataID),
					"SkinningRenderData already inserted. This should not be possible");

				m_meshIDToSkinJoints.emplace(newRenderDataID, nullptr); // We'll populate this later
			}
		}

		// Update buffers for dirty SkinningRenderData:
		if (renderData.HasObjectData<gr::MeshPrimitive::SkinningRenderData>())
		{
			std::vector<gr::RenderDataID> const* dirtySkinningRenderData =
				renderData.GetIDsWithDirtyData<gr::MeshPrimitive::SkinningRenderData>();
			if (dirtySkinningRenderData)
			{
				for (auto const& dirtySkinRenderDataItr : gr::IDAdapter(renderData, *dirtySkinningRenderData))
				{
					const gr::RenderDataID meshRenderDataID = dirtySkinRenderDataItr->GetRenderDataID();
					SEAssert(m_meshIDToSkinJoints.contains(meshRenderDataID),
						"SkinningRenderData not found. This should not be possible");

					gr::MeshPrimitive::SkinningRenderData const& skinRenderData =
						dirtySkinRenderDataItr->Get<gr::MeshPrimitive::SkinningRenderData>();

					// Create/update the skinning joints buffer:
					std::shared_ptr<re::Buffer>& skinJointsBuffer = m_meshIDToSkinJoints.at(meshRenderDataID);

					if (skinJointsBuffer == nullptr)
					{
						skinJointsBuffer = re::Buffer::CreateUncommittedArray<SkinningJoint>(
							std::format("SkinningRenderData {} Joints", meshRenderDataID),
							re::Buffer::BufferParams{
								.m_stagingPool = re::Buffer::StagingPool::Permanent,
								.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
								.m_accessMask = re::Buffer::Access::GPURead,
								.m_usageMask = re::Buffer::Usage::Structured,
								.m_arraySize = util::CheckedCast<uint32_t>(skinRenderData.m_jointTransforms.size()),
							});

						// Force the update for newly created buffers, as there is no guarantee the associated
						// Transforms are dirty
						UpdateSkinningJointsBuffer(skinRenderData, skinJointsBuffer);
					}					
				}
			}
		}


		// Remove Buffers/VertexBufferInputs for deleted MeshPrimitive RenderDataIDs:
		std::vector<gr::RenderDataID> const* deletedMeshPrimitiveRenderDataIDs = 
			renderData.GetIDsWithDeletedData<gr::MeshPrimitive::RenderData>();
		if (deletedMeshPrimitiveRenderDataIDs)
		{
			for (gr::RenderDataID deletedID : *deletedMeshPrimitiveRenderDataIDs)
			{
				RemoveAnimationBuffers(deletedID);
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

				SEAssert((!meshPrimRenderData.m_meshHasSkinning && !meshPrimRenderData.m_hasMorphTargets) ||
					meshPrimRenderData.m_meshHasSkinning != meshPrimRenderData.m_hasMorphTargets,
					"TODO: Support vertex animation when both morph targets and skinning are enabled. For now, we "
					"assume only one or the other is enabled");

				if (meshPrimRenderData.m_hasMorphTargets || meshPrimRenderData.m_meshHasSkinning)
				{
					AddAnimationBuffers(newRenderDataID, meshPrimRenderData);
				}
			}
		}

		// Create batches, if necessary:
		if (!m_meshIDToMorphWeights.empty())
		{
			if (m_viewCullingResults)
			{
				for (auto const& viewAndCulledIDs : *m_viewCullingResults)
				{
					CreateMorphAnimationBatches(gr::IDAdapter(renderData, viewAndCulledIDs.second));
				}
			}
			else
			{
				CreateMorphAnimationBatches(gr::ObjectAdapter<gr::MeshPrimitive::RenderData>(renderData));
			}
		}

		if (!m_meshIDToSkinJoints.empty())
		{
			std::unordered_set<gr::RenderDataID> seenSkinnedMeshes;

			if (m_viewCullingResults)
			{
				for (auto const& viewAndCulledIDs : *m_viewCullingResults)
				{
					CreateSkinningAnimationBatches(
						gr::IDAdapter(renderData, viewAndCulledIDs.second),
						seenSkinnedMeshes);
				}
			}
			else
			{
				CreateSkinningAnimationBatches(
					gr::ObjectAdapter<gr::MeshPrimitive::RenderData>(renderData),
					seenSkinnedMeshes);
			}			
		}
	}


	void VertexAnimationGraphicsSystem::CreateMorphAnimationBatches(auto&& renderDataItr)
	{
		for (auto const& itr : renderDataItr)
		{
			if (itr->HasObjectData<gr::MeshPrimitive::RenderData>())
			{
				const gr::RenderDataID curID = itr->GetRenderDataID();

				gr::MeshPrimitive::RenderData const& meshPrimRenderData = itr->Get<gr::MeshPrimitive::RenderData>();

				// Dispatch a compute batch:
				if (meshPrimRenderData.m_hasMorphTargets)
				{
					SEAssert(m_meshPrimIDToAnimBuffers.contains(curID),
						"Failed to find a destination vertex buffer to write to. This should not be possible");

					SEAssert(m_meshIDToMorphWeights.contains(meshPrimRenderData.m_owningMeshRenderDataID),
						"MeshPrimitive has an owning Mesh ID that hasn't been registered. This shouldn't be possible");

					const uint32_t numVerts = meshPrimRenderData.m_vertexStreams[0]->GetNumElements();
					SEAssert(numVerts >= 3, "Less than 3 verts. This is unexpected");

					// We process verts in 1D (round up to ensure we dispatch at least one thread group)
					const uint32_t roundedXDim =
						(numVerts / VERTEX_ANIM_THREADS_X) + ((numVerts % VERTEX_ANIM_THREADS_X) == 0 ? 0 : 1);

					AnimationBuffers const& animBuffers = m_meshPrimIDToAnimBuffers.at(curID);

					// Process our streams in blocks (OpenGL limits the no. of SSBO's are accessible at once)
					const uint8_t numDispatches = util::RoundUpToNearestMultiple(
						animBuffers.m_numAnimatedStreams,
						static_cast<uint8_t>(MAX_STREAMS_PER_DISPATCH)) / MAX_STREAMS_PER_DISPATCH;

					for (uint8_t dispatchIdx = 0; dispatchIdx < numDispatches; ++dispatchIdx)
					{
						gr::ComputeBatchBuilder morphBatchBuilder = gr::ComputeBatchBuilder()
							.SetThreadGroupCount(glm::uvec3(roundedXDim, 1u, 1u))
							.SetEffectID(k_vertexAnimationEffectID)
							.SetBuffer("MorphWeights", // AnimationData: Per-Mesh weights
								m_meshIDToMorphWeights.at(meshPrimRenderData.m_owningMeshRenderDataID));

						// Attach input/output vertex buffers:
						auto const& animBuffers = m_meshPrimIDToAnimBuffers.at(curID);

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

							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float ||
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float2 ||
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float3 ||
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float4,
								"We're expecing our position data will be stored as FloatNs");

							// We view our data as arrays of floats:
							const uint32_t numFloatElements =
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements() *
								DataTypeToNumComponents(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType());
							constexpr uint32_t k_floatStride = sizeof(float);

							// Set the input vertex stream buffers:
							std::move(morphBatchBuilder).SetBuffer(
								"InVertexStreams",
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetBufferSharedPtr(),
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numFloatElements,
									.m_structuredByteStride = k_floatStride,
									.m_firstDestIdx = bufferShaderIdx,
								});

							// Set the output vertex stream buffers:
							std::move(morphBatchBuilder).SetBuffer(
								"OutVertexStreams",
								animBuffers.m_destBuffers[srcIdx],
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numFloatElements,
									.m_structuredByteStride = k_floatStride,
									.m_firstDestIdx = bufferShaderIdx,
								});

							++bufferShaderIdx;
						}

						// Set the vertex stream metadata:
						std::move(morphBatchBuilder).SetBuffer(
							"MorphMetadataParams",
							m_meshPrimIDToAnimBuffers.at(curID).m_morphMetadataBuffer);

						// Set the interleaved morph data:
						std::move(morphBatchBuilder).SetBuffer(
							"MorphData",
							meshPrimRenderData.m_interleavedMorphData);

						gr::StageBatchHandle* morphBatch = 
							m_morphAnimationStage->AddBatch(std::move(morphBatchBuilder).Build());

						// Set the dispatch metadata:
						morphBatch->SetSingleFrameBuffer(MorphDispatchMetadata::s_shaderName,
							re::Buffer::Create(
								MorphDispatchMetadata::s_shaderName,
								GetMorphDispatchMetadataData(bufferShaderIdx),
								re::Buffer::BufferParams{
									.m_lifetime = re::Lifetime::SingleFrame,
									.m_stagingPool = re::Buffer::StagingPool::Temporary,
									.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
									.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
									.m_usageMask = re::Buffer::Usage::Constant
								}));
					}
				}
			}
		}
	}


	void VertexAnimationGraphicsSystem::CreateSkinningAnimationBatches(
		auto&& renderDataItr, std::unordered_set<gr::RenderDataID>& seenIDs)
	{
		for (auto const& itr : renderDataItr)
		{
			if (itr->HasObjectData<gr::MeshPrimitive::RenderData>())
			{
				const gr::RenderDataID curID = itr->GetRenderDataID();

				gr::MeshPrimitive::RenderData const& meshPrimRenderData = itr->Get<gr::MeshPrimitive::RenderData>();

				// Dispatch a compute batch:
				if (meshPrimRenderData.m_meshHasSkinning)
				{
					SEAssert(m_meshPrimIDToAnimBuffers.contains(curID),
						"Failed to find a destination vertex buffer to write to. This should not be possible");

					SEAssert(itr->GetRenderDataManager()->HasObjectData<gr::MeshPrimitive::SkinningRenderData>(
						meshPrimRenderData.m_owningMeshRenderDataID),
						"Owning mesh does not have skinning render data. This should not be possible");

					SEAssert(m_meshIDToSkinJoints.contains(meshPrimRenderData.m_owningMeshRenderDataID),
						"Failed to find skinning joints buffer for the owning Mesh. This should not be possible");

					// Only update skinning joints for Meshes that have a MeshPrimitive that passed culling, and 
					// only update them once
					auto const& jointBufferItr = m_meshIDToSkinJoints.find(meshPrimRenderData.m_owningMeshRenderDataID);
					if (jointBufferItr != m_meshIDToSkinJoints.end() &&
						!seenIDs.contains(meshPrimRenderData.m_owningMeshRenderDataID) &&
						itr->GetRenderDataManager()->IsDirty<gr::MeshPrimitive::SkinningRenderData>(
							meshPrimRenderData.m_owningMeshRenderDataID))
					{						
						gr::MeshPrimitive::SkinningRenderData const& skinningData = 
							itr->GetRenderDataManager()->GetObjectData<gr::MeshPrimitive::SkinningRenderData>(
								meshPrimRenderData.m_owningMeshRenderDataID);

						UpdateSkinningJointsBuffer(
							skinningData,
							jointBufferItr->second);

						seenIDs.emplace(meshPrimRenderData.m_owningMeshRenderDataID);
					}

					const uint32_t numVerts = meshPrimRenderData.m_vertexStreams[0]->GetNumElements();
					SEAssert(numVerts >= 3, "Less than 3 verts. This is unexpected");

					// We process verts in 1D (round up to ensure we dispatch at least one thread group)
					const uint32_t roundedXDim =
						(numVerts / VERTEX_ANIM_THREADS_X) + ((numVerts % VERTEX_ANIM_THREADS_X) == 0 ? 0 : 1);

					AnimationBuffers const& animBuffers = m_meshPrimIDToAnimBuffers.at(curID);

					gr::ComputeBatchBuilder skinningBatchBuilder = gr::ComputeBatchBuilder()
						.SetThreadGroupCount(glm::uvec3(roundedXDim, 1u, 1u))
						.SetEffectID(k_vertexAnimationEffectID);

					// Track the streams we've seen for debug validation:
					bool seenPosition = false;
					bool seenNormal = false;
					bool seenTangent = false;
					bool seenBlendIndices = false;
					bool seenBlendWeights = false;

					// Attach input/output vertex buffers:
					for (uint8_t srcIdx = 0; srcIdx < meshPrimRenderData.m_numVertexStreams; ++srcIdx)
					{
						const re::VertexStream::Type streamType =
							meshPrimRenderData.m_vertexStreams[srcIdx]->GetType();

						char const* inShaderName = nullptr;
						char const* outShaderName = nullptr;
						uint32_t numElements = 0;

						// Set the input and output vertex stream buffers:
						switch (streamType)
						{
						case re::VertexStream::Type::Position:
						{
							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float3,
								"We're expecing our position data will be stored as Float3s");

							SEAssert(!seenPosition, "Found multiple position streams. This is unexpected");
							seenPosition = true;

							constexpr char const* k_posInShaderName = "InPosition";
							constexpr char const* k_posOutShaderName = "OutPosition";

							inShaderName = k_posInShaderName;
							outShaderName = k_posOutShaderName;
							numElements = meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements();
						}
						break;
						case re::VertexStream::Type::Normal:
						{
							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float3,
								"We're expecing our normal data will be stored as Float3s");

							SEAssert(!seenNormal, "Found multiple normal streams. This is unexpected");
							seenNormal = true;

							constexpr char const* k_normalInShaderName = "InNormal";
							constexpr char const* k_normalOutShaderName = "OutNormal";

							inShaderName = k_normalInShaderName;
							outShaderName = k_normalOutShaderName;
							numElements = meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements();
						}
						break;
						case re::VertexStream::Type::Tangent:
						{
							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float4,
								"We're expecing our tangent data will be stored as Float4s");

							SEAssert(!seenTangent, "Found multiple tangent streams. This is unexpected");
							seenTangent = true;

							constexpr char const* k_tangentInShaderName = "InTangent";
							constexpr char const* k_tangentOutShaderName = "OutTangent";

							inShaderName = k_tangentInShaderName;
							outShaderName = k_tangentOutShaderName;
							numElements = meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements();
						}
						break;
						case re::VertexStream::Type::BlendIndices:
						{
							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float4,
								"We're expecing our joint indexes will be stored as Float4s");

							SEAssert(!seenBlendIndices, "Found multiple blend index streams. TODO: Support this");
							seenBlendIndices = true;

							constexpr char const* k_blendIndicesInShaderName = "InBlendIndices";

							inShaderName = k_blendIndicesInShaderName;

							// We view our joints indices as arrays of floats:
							numElements = meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements() *
								DataTypeToNumComponents(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType());
						}
						break;
						case re::VertexStream::Type::BlendWeight:
						{
							SEAssert(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType() == re::DataType::Float4,
								"We're expecing our blend weights will be stored as Float4s");

							SEAssert(!seenBlendWeights, "Found multiple blend weights streams. TODO: Support this");
							seenBlendWeights = true;

							constexpr char const* k_blendWeightsInShaderName = "InBlendWeights";

							inShaderName = k_blendWeightsInShaderName;

							// We view our weights as arrays of floats
							numElements = meshPrimRenderData.m_vertexStreams[srcIdx]->GetNumElements() *
								DataTypeToNumComponents(meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType());
						}
						break;
						default: continue;
						}

						// Attach vertex buffers:
						if (inShaderName)
						{
							std::move(skinningBatchBuilder).SetBuffer(
								inShaderName,
								meshPrimRenderData.m_vertexStreams[srcIdx]->GetBufferSharedPtr(),
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numElements,
									.m_structuredByteStride = re::DataTypeToByteStride(
										meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType()),
								});
						}

						if (outShaderName)
						{
							std::move(skinningBatchBuilder).SetBuffer(
								outShaderName,
								animBuffers.m_destBuffers[srcIdx],
								re::BufferView::BufferType{
									.m_firstElement = 0,
									.m_numElements = numElements,
									.m_structuredByteStride = re::DataTypeToByteStride(
										meshPrimRenderData.m_vertexStreams[srcIdx]->GetDataType()),
								});
						}
					}

					// Set the MeshPrimitive skinning buffers::
					std::move(skinningBatchBuilder).SetBuffer(SkinningData::s_shaderName,
						m_meshPrimIDToAnimBuffers.at(curID).m_skinningDataBuffer);

					// Set the Mesh skinning buffers:
					std::move(skinningBatchBuilder).SetBuffer("SkinningMatrices",
						m_meshIDToSkinJoints.at(meshPrimRenderData.m_owningMeshRenderDataID));

					m_skinAnimationStage->AddBatch(std::move(skinningBatchBuilder).Build());
				}
			}
		}
	}


	void VertexAnimationGraphicsSystem::AddAnimationBuffers(
		gr::RenderDataID renderDataID, gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		SEAssert(!m_meshPrimIDToAnimBuffers.contains(renderDataID) && !m_outputs.contains(renderDataID),
			"RenderDataID has already been registered. This should not be possible");

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		const bool hasSkinningData = meshPrimRenderData.m_meshHasSkinning;
		const bool hasMorphData = meshPrimRenderData.m_hasMorphTargets;

		SEAssert(hasSkinningData != hasMorphData,
			"TODO: Support vertex animation when both morph targets and skinning are enabled. For now, we "
			"assume only one or the other is enabled");

		// Insert new entries for our output buffers/data:
		auto destVertexBuffers = m_meshPrimIDToAnimBuffers.emplace(renderDataID, AnimationBuffers{});

		auto newOutputs = m_outputs.emplace(
			renderDataID, std::array<re::VertexBufferInput, re::VertexStream::k_maxVertexStreams>{});


		for (uint8_t streamIdx = 0; streamIdx < re::VertexStream::k_maxVertexStreams; ++streamIdx)
		{
			if (meshPrimRenderData.m_vertexStreams[streamIdx] == nullptr)
			{
				break;
			}

			const re::VertexStream::Type streamType = meshPrimRenderData.m_vertexStreams[streamIdx]->GetType();

			bool needDestBuffer = false;
			std::string destBufferName;

			if (hasMorphData && !hasSkinningData) // Morph targets only
			{
				// If we've got morph data, create an output buffer to write into
				if (meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_byteStride != 0 ||
					meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_numComponents != 0)
				{
					SEAssert(meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_byteStride != 0 &&
						meshPrimRenderData.m_morphTargetMetadata.m_perStreamMetadata[streamIdx].m_numComponents != 0,
						"Byte stride and number of components out of sync: Both should be mutually zero/non-zero");

					destBufferName = std::format(
						"MorphVerts: MeshPrim ID {}, stream {}: {}, Hash:{}",
						renderDataID,
						streamIdx,
						re::VertexStream::TypeToCStr(streamType),
						meshPrimRenderData.m_vertexStreams[streamIdx]->GetDataHash());

					needDestBuffer = true;
				}
			}
			else if (!hasMorphData && hasSkinningData) // Skinning only
			{
				if (streamType == re::VertexStream::Type::Position ||
					streamType == re::VertexStream::Type::Normal ||
					streamType == re::VertexStream::Type::Tangent)
				{
					destBufferName = std::format(
						"SkinnedVerts: MeshPrim ID {}, stream {}: {}, Hash:{}",
						renderDataID,
						streamIdx,
						re::VertexStream::TypeToCStr(streamType),
						meshPrimRenderData.m_vertexStreams[streamIdx]->GetDataHash());

					needDestBuffer = true;
				}
			}
			else if (hasMorphData && hasSkinningData) // Morph targets and skinning
			{
				SEAssertF("TODO: Handle this case by writing morph outputs to an intermediate buffer, and using them "
					"as an input to the skinning stage");
			}

			if (needDestBuffer)
			{
				// Create a destination buffer for our animated vertices:
				destVertexBuffers.first->second.m_destBuffers[streamIdx] = re::Buffer::CreateUnstaged(
					destBufferName,
					meshPrimRenderData.m_vertexStreams[streamIdx]->GetTotalDataByteSize(),
					re::Buffer::BufferParams{
						.m_stagingPool = re::Buffer::StagingPool::None,
						.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
						.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::GPUWrite,
						.m_usageMask = re::Buffer::Structured | re::Buffer::Usage::Raw,
					});

				// Create a stream view matching the configuration of the VertexStream, but with our new buffer
				newOutputs.first->second[streamIdx] = re::VertexBufferInput(
					meshPrimRenderData.m_vertexStreams[streamIdx],
					destVertexBuffers.first->second.m_destBuffers[streamIdx].get());

				destVertexBuffers.first->second.m_numAnimatedStreams++;
			}
			else // Stream is not animated: Just pass it through
			{
				newOutputs.first->second[streamIdx] = re::VertexBufferInput(meshPrimRenderData.m_vertexStreams[streamIdx]);
			}
		}

		// Morph target buffers:
		if (hasMorphData)
		{
			// Mesh primitive metadata:
			destVertexBuffers.first->second.m_morphMetadataBuffer = re::Buffer::Create(
				std::format("MeshPrimitiveID {} MorphMetadata", renderDataID),
				GetMorphMetadataData(meshPrimRenderData),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
					.m_accessMask = re::Buffer::Access::GPURead,
					.m_usageMask = re::Buffer::Usage::Constant,
				});
		}

		// Skinning buffers:
		if (hasSkinningData)
		{
			AnimationBuffers& animBuffers = destVertexBuffers.first->second;

			animBuffers.m_skinningDataBuffer = re::Buffer::Create(
				std::format("MeshPrimitiveID {} SkinningData", renderDataID),
				GetSkinningData(meshPrimRenderData),
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::Permanent,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::MemoryPoolPreference::DefaultHeap,
					.m_accessMask = re::Buffer::Access::GPURead,
					.m_usageMask = re::Buffer::Usage::Constant,
				});

			gr::MeshPrimitive::SkinningRenderData const& skinRenderData = 
				renderData.GetObjectData<gr::MeshPrimitive::SkinningRenderData>(meshPrimRenderData.m_owningMeshRenderDataID);
		}
	}


	void VertexAnimationGraphicsSystem::RemoveAnimationBuffers(gr::RenderDataID renderDataID)
	{
		m_meshPrimIDToAnimBuffers.erase(renderDataID);
		m_outputs.erase(renderDataID);
	}
}