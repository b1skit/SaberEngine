// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BoundsRenderData.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "LightRenderData.h"
#include "TransformRenderData.h"

#include "Core/Util/ByteVector.h"
#include "Core/Util/ImGuiUtils.h"

#include "Shaders/Common/DebugParams.h"


namespace
{
	static const EffectID k_debugEffectID = effect::Effect::ComputeEffectID("Debug");


	std::unique_ptr<re::Batch> BuildAxisBatch(
		gr::RenderDataID renderDataID,
		re::Lifetime batchLifetime)
	{
		util::ByteVector axisOriginPos = util::ByteVector::Create<glm::vec3>({ glm::vec3(0.f, 0.f, 0.f) });

		core::InvPtr<gr::VertexStream> const& axisPositionStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = batchLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(axisOriginPos),
			false);

		re::Batch::GraphicsParams axisBatchGraphicsParams;
		axisBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		axisBatchGraphicsParams.m_numInstances = 1;
		axisBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::PointList;
		axisBatchGraphicsParams.m_vertexBuffers = { re::VertexBufferInput(axisPositionStream) };

		std::unique_ptr<re::Batch> axisBatch = std::make_unique<re::Batch>(
			batchLifetime, axisBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Axis);

		return axisBatch;
	}


	std::unique_ptr<re::Batch> BuildParentChildLinkBatch(
		gr::RenderDataManager const& renderData,
		re::Lifetime lifetime,
		glm::vec4 const& parentColor,
		glm::vec4 const& childColor,
		gr::TransformID parentTransformID,
		gr::RenderDataID childRenderDataID)
	{
		constexpr glm::vec4 k_originPoint(0.f, 0.f, 0.f, 1.f);

		core::InvPtr<gr::VertexStream> const& linePositionsStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = lifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			util::ByteVector::Create<glm::vec3>({ k_originPoint, k_originPoint }), // [0] = parent, [1] = child
			false);

		core::InvPtr<gr::VertexStream> const& lineColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = lifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			util::ByteVector::Create<glm::vec4>({ parentColor, childColor }),
			false);

		core::InvPtr<gr::VertexStream> const& lineIndexStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = lifetime,
				.m_type = gr::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			util::ByteVector::Create<uint16_t>({ 0, 1 }),
			false);

		re::Batch::GraphicsParams lineBatchGraphicsParams;
		lineBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		lineBatchGraphicsParams.m_numInstances = 1;
		lineBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::LineList;
		lineBatchGraphicsParams.m_vertexBuffers = {linePositionsStream, lineColorStream };
		lineBatchGraphicsParams.m_indexBuffer = re::VertexBufferInput(lineIndexStream);

		std::unique_ptr<re::Batch> lineBatch = std::make_unique<re::Batch>(
			lifetime, lineBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_VertexIDInstancingLUTIdx);

		return lineBatch;
	}


	std::unique_ptr<re::Batch> BuildBoundingBoxBatch(
		gr::RenderDataManager const& renderData,
		gr::RenderDataID renderDataID, 
		re::Lifetime batchLifetime,
		gr::Bounds::RenderData const& bounds,
		glm::vec4 const& boxColor)
	{
		/* Construct a cube from 8 points:
		*     e----f
		*    / |  /|
		*	a----b |
		*   |  | | |
		*   |  g---h
		*   |/   |/
		* 	c----d	
		*/
		const float xMin = bounds.m_worldMinXYZ.x;
		const float yMin = bounds.m_worldMinXYZ.y;
		const float zMin = bounds.m_worldMinXYZ.z;

		const float xMax = bounds.m_worldMaxXYZ.x;
		const float yMax = bounds.m_worldMaxXYZ.y;
		const float zMax = bounds.m_worldMaxXYZ.z;

		const glm::vec3 a = glm::vec3(xMin, yMax, zMax);
		const glm::vec3 b = glm::vec3(xMax, yMax, zMax);
		const glm::vec3 c = glm::vec3(xMin, yMin, zMax);
		const glm::vec3 d = glm::vec3(xMax, yMin, zMax);

		const glm::vec3 e = glm::vec3(xMin, yMax, zMin);
		const glm::vec3 f = glm::vec3(xMax, yMax, zMin);
		const glm::vec3 g = glm::vec3(xMin, yMin, zMin);
		const glm::vec3 h = glm::vec3(xMax, yMin, zMin);

		//																	 0, 1, 2, 3, 4, 5, 6, 7
		util::ByteVector boxPositions = util::ByteVector::Create<glm::vec3>({a, b, c, d, e, f, g, h});

		util::ByteVector boxColors = util::ByteVector::Create<glm::vec4>(boxPositions.size(), boxColor);

		util::ByteVector boxIndexes = util::ByteVector::Create<uint16_t>({
			// Front face:
			0, 2,
			2, 3,
			3, 1,
			1, 0,

			// Back face:
			4, 6,
			6, 7, 
			7, 5, 
			5, 4,

			// Left side: Connect edges between front/back faces
			4, 0,
			6, 2,

			// Right side: Connect edges between front/back faces
			5, 1,
			7, 3
		});

		core::InvPtr<gr::VertexStream> const& boxPositionsStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = batchLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(boxPositions),
			false);

		core::InvPtr<gr::VertexStream> const& boxColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = batchLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(boxColors),
			false);

		core::InvPtr<gr::VertexStream> const& boxIndexStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = batchLifetime,
				.m_type = gr::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			std::move(boxIndexes),
			false);

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::LineList;

		boundingBoxBatchGraphicsParams.m_vertexBuffers[0] = boxPositionsStream;
		boundingBoxBatchGraphicsParams.m_vertexBuffers[1] = boxColorStream;

		boundingBoxBatchGraphicsParams.m_indexBuffer = re::VertexBufferInput(boxIndexStream);

		std::unique_ptr<re::Batch> boundingBoxBatch = std::make_unique<re::Batch>(
			batchLifetime, boundingBoxBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Line);

		return boundingBoxBatch;
	}


	std::unique_ptr<re::Batch> BuildVertexNormalsBatch(
		gr::RenderDataID renderDataID,
		re::Lifetime batchLifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		core::InvPtr<gr::VertexStream> const& normalStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
				meshPrimRenderData, gr::VertexStream::Type::Normal);
		if (normalStream == nullptr)
		{
			return nullptr; // No normals? Nothing to build
		}

		core::InvPtr<gr::VertexStream> const& positionStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
			meshPrimRenderData, gr::VertexStream::Type::Position);
		SEAssert(positionStream, "Cannot find position stream");

		SEAssert(positionStream->GetDataType() == re::DataType::Float3 && 
			normalStream->GetDataType() == re::DataType::Float3,
			"Unexpected position or normal data");

		re::Batch::GraphicsParams normalBatchGraphicsParams;
		normalBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		normalBatchGraphicsParams.m_numInstances = 1;
		normalBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::PointList;
		normalBatchGraphicsParams.m_vertexBuffers = { positionStream, normalStream, };

		std::unique_ptr<re::Batch> normalDebugBatch = std::make_unique<re::Batch>(
			batchLifetime, normalBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Normal);

		return normalDebugBatch;
	}

	
	std::unique_ptr<re::Batch> BuildCameraFrustumBatch(
		re::Lifetime batchLifetime,
		gr::Transform::RenderData const* transformData,
		glm::vec4 const& frustumColor)
	{
		// NDC coordinates:
		glm::vec4 farTL = glm::vec4(-1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBL = glm::vec4(-1.f, -1.f, 1.f, 1.f);
		glm::vec4 farTR = glm::vec4(1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBR = glm::vec4(1.f, -1.f, 1.f, 1.f);
		glm::vec4 nearTL = glm::vec4(-1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBL = glm::vec4(-1.f, -1.f, 0.f, 1.f);
		glm::vec4 nearTR = glm::vec4(1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBR = glm::vec4(1.f, -1.f, 0.f, 1.f);

		util::ByteVector frustumPositions = util::ByteVector::Create<glm::vec3>(
			{ farTL, farBL, farTR, farBR, nearTL, nearBL, nearTR, nearBR });
		//	  0		1		2	   3	  4		  5		  6		  7

		util::ByteVector frustumColors = util::ByteVector::Create( frustumPositions.size(), frustumColor);

		util::ByteVector frustumIndexes = util::ByteVector::Create<uint16_t>({
			// Back face:
			0, 1,
			1, 3,
			3, 2,
			2, 0,

			// Front face:
			4, 5,
			5, 7,
			7, 6,
			6, 4,
			
			// Left face: Connecting edges from the front/back faces
			0, 4,
			1, 5,

			// Right face: Connecting edges from the front/back faces
			2, 6,
			3, 7
		});

		const re::Lifetime streamLifetime = batchLifetime;

		core::InvPtr<gr::VertexStream> const& frustumPositionsStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(frustumPositions),
			false);

		core::InvPtr<gr::VertexStream> const& frustumColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(frustumColors),
			false);

		core::InvPtr<gr::VertexStream> const& frustumIndexStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			std::move(frustumIndexes),
			false);

		re::Batch::GraphicsParams frustumBatchGraphicsParams{};
		frustumBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		frustumBatchGraphicsParams.m_numInstances = 1;
		frustumBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::LineList;

		frustumBatchGraphicsParams.m_vertexBuffers[0] = frustumPositionsStream;
		frustumBatchGraphicsParams.m_vertexBuffers[1] = frustumColorStream;

		frustumBatchGraphicsParams.m_indexBuffer = frustumIndexStream;

		std::unique_ptr<re::Batch> frustumBatch = std::make_unique<re::Batch>(
			batchLifetime, frustumBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_InstanceIDTransformIdx);

		return frustumBatch;
	}
	

	std::unique_ptr<re::Batch> BuildWireframeBatch(
		gr::RenderDataID renderDataID,
		re::Lifetime batchLifetime, 
		gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		core::InvPtr<gr::VertexStream> const& positionStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
			meshPrimRenderData, gr::VertexStream::Type::Position);

		core::InvPtr<gr::VertexStream> const& indexStream = meshPrimRenderData.m_indexStream;
		SEAssert(positionStream && indexStream, "Must have a position and index stream");

		re::Batch::GraphicsParams wireframeBatchGraphicsParams;
		wireframeBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		wireframeBatchGraphicsParams.m_numInstances = 1;
		wireframeBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;
		wireframeBatchGraphicsParams.m_vertexBuffers = {positionStream};
		wireframeBatchGraphicsParams.m_indexBuffer = indexStream;

		std::unique_ptr<re::Batch> wireframeBatch = std::make_unique<re::Batch>(
			batchLifetime, wireframeBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Wireframe);

		return wireframeBatch;
	}
	

	glm::mat4 AdjustMat4Scale(glm::mat4 const& mat, glm::vec3 const& matScale, float newUniformScale = 1.f)
	{
		// Remove the scale from the basis vectors, and factor in the (optional) new uniform scale factor
		glm::mat4 result = mat;
		result[0] *= newUniformScale / matScale.x;
		result[1] *= newUniformScale / matScale.y;
		result[2] *= newUniformScale / matScale.z;
		return result;
	}
}


namespace gr
{
	void DebugGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_viewBatchesDataInput);
	}


	DebugGraphicsSystem::DebugGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_isDirty(true)
	{
	}


	void DebugGraphicsSystem::InitPipeline(
		re::StagePipeline& stagePipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_debugParams = re::BufferInput(
			DebugData::s_shaderName,
			re::Buffer::Create(
				DebugData::s_shaderName,
				PackDebugData(),
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::CPUWrite | re::Buffer::GPURead,
					.m_usageMask = re::Buffer::Constant,
					.m_arraySize = 1,
				}));

		m_debugStage = re::Stage::CreateGraphicsStage("Debug stage", re::Stage::GraphicsStageParams{});
		
		m_debugStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer
		m_debugStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_debugStage->AddPermanentBuffer(m_debugParams);

		stagePipeline.AppendStage(m_debugStage);


		m_wireframeStage = re::Stage::CreateGraphicsStage("Debug: Wireframe stage", re::Stage::GraphicsStageParams{});

		m_wireframeStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer
		m_wireframeStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_wireframeStage->AddPermanentBuffer(m_debugParams);
		m_wireframeStage->SetDrawStyle(effect::drawstyle::Debug_Wireframe);

		stagePipeline.AppendStage(m_wireframeStage);

		// Cache our dependencies:
		m_viewBatches = GetDataDependency<ViewBatches>(k_viewBatchesDataInput, dataDependencies);
		SEAssert(m_viewBatches, "Must have received some batches");
	}


	void DebugGraphicsSystem::PreRender()
	{
		CreateBatches();

		if (m_isDirty)
		{
			m_debugParams.GetBuffer()->Commit(PackDebugData());
			m_isDirty = false;
		}
	}


	void DebugGraphicsSystem::CreateBatches()
	{
		constexpr glm::mat4 k_identity = glm::mat4(1.f);

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		if (m_showWorldCoordinateAxis)
		{
			// Use the 1st RenderDataID that references the identity transform to obtain a view
			std::vector<gr::RenderDataID> const& identityObjects =
				renderData.GetRenderDataIDsReferencingTransformID(k_invalidTransformID);
			SEAssert(!identityObjects.empty(), "No RenderDataIDs associated with the identity transform");

			const gr::RenderDataID identityID = identityObjects[0];
			if (m_worldCoordinateAxisBatch == nullptr)
			{
				m_worldCoordinateAxisBatch = BuildAxisBatch(identityObjects[0], re::Lifetime::Permanent);
			}
			re::Batch* batch = m_debugStage->AddBatchWithLifetime(*m_worldCoordinateAxisBatch, re::Lifetime::SingleFrame);

			batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
				InstanceIndexData::s_shaderName, std::views::single(identityID)));
			batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
		}
		else
		{
			m_worldCoordinateAxisBatch = nullptr;
		}

		if (m_showAllWireframe)
		{
			const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
			if (mainCamID != gr::k_invalidRenderDataID)
			{
				SEAssert(m_viewBatches->contains(mainCamID), "Cannot find main camera ID in view batches");

				std::vector<re::Batch> const& mainCamBatches = m_viewBatches->at(mainCamID);
				for (re::Batch const& batch : mainCamBatches)
				{
					m_wireframeStage->AddBatch(batch);
				}
			}
		}

		if (m_showAllMeshPrimitiveBounds || 
			m_showMeshCoordinateAxis || 
			m_showAllVertexNormals)
		{
			for (auto const& meshPrimItr : gr::ObjectAdapter<gr::MeshPrimitive::RenderData, gr::Bounds::RenderData>(
				renderData, gr::RenderObjectFeature::IsMeshPrimitiveConcept))
			{
				const gr::RenderDataID meshPrimRenderDataID = meshPrimItr->GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(meshPrimRenderDataID))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
						meshPrimItr->Get<gr::MeshPrimitive::RenderData>();
					gr::Bounds::RenderData const& boundsRenderData = meshPrimItr->Get<gr::Bounds::RenderData>();

					gr::Transform::RenderData const& transformData = meshPrimItr->GetTransformData();


					// MeshPrimitives:
					if (m_showAllMeshPrimitiveBounds || m_showAllVertexNormals)
					{
						if (m_showAllMeshPrimitiveBounds && 
							gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveBounds, meshPrimItr->GetFeatureBits()))
						{
							if (!m_meshPrimBoundsBatches.contains(meshPrimRenderDataID) ||
								meshPrimItr->IsDirty<gr::Bounds::RenderData>())
							{
								m_meshPrimBoundsBatches[meshPrimRenderDataID] =
									BuildBoundingBoxBatch(
										renderData,
										meshPrimRenderDataID, 
										re::Lifetime::Permanent, 
										boundsRenderData, 
										m_meshPrimBoundsColor);
							}
							re::Batch* batch = m_debugStage->AddBatchWithLifetime(
								*m_meshPrimBoundsBatches.at(meshPrimRenderDataID), re::Lifetime::SingleFrame);

							// For simplicity, we build our lines in world space, and attach an identity transform buffer
							std::vector<gr::RenderDataID> const& associatedRenderDataIDs =
								renderData.GetRenderDataIDsReferencingTransformID(gr::k_invalidTransformID);
							SEAssert(!associatedRenderDataIDs.empty(),
								"No RenderDataIDs assocated with the parent TransformID");

							batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
									InstanceIndexData::s_shaderName, std::views::single(associatedRenderDataIDs[0])));
							batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
						}

						if (m_showAllVertexNormals)
						{
							if (!m_vertexNormalBatches.contains(meshPrimRenderDataID))
							{
								std::unique_ptr<re::Batch> normalsBatch = BuildVertexNormalsBatch(
										meshPrimRenderDataID,
										re::Lifetime::Permanent,
										meshPrimRenderData);

								if (normalsBatch)
								{
									m_vertexNormalBatches.emplace(
										meshPrimRenderDataID,
										std::move(normalsBatch));
								}
							}
							re::Batch* batch = m_debugStage->AddBatchWithLifetime(
								*m_vertexNormalBatches.at(meshPrimRenderDataID), re::Lifetime::SingleFrame);
							batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, std::views::single(meshPrimRenderDataID)));
							batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
						}
					}

					if (m_showMeshCoordinateAxis)
					{
						if (!m_meshCoordinateAxisBatches.contains(meshPrimRenderDataID))
						{
							m_meshCoordinateAxisBatches.emplace(
								meshPrimRenderDataID, 
								BuildAxisBatch(meshPrimRenderDataID, re::Lifetime::Permanent));
						}
						re::Batch* batch = m_debugStage->AddBatchWithLifetime(
							*m_meshCoordinateAxisBatches.at(meshPrimRenderDataID), re::Lifetime::SingleFrame);
						batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
							InstanceIndexData::s_shaderName, std::views::single(meshPrimRenderDataID)));
						batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
					}
				}
			}
		}
		else
		{
			m_meshPrimBoundsBatches.clear();
			m_vertexNormalBatches.clear();
			m_meshCoordinateAxisBatches.clear();
		}

		auto HandleBoundsBatches = [this, &renderData, &ibm](
			bool doShowBounds,
			gr::RenderObjectFeature boundsFeatureBit, 
			std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>>& boundsBatches,
			glm::vec4 const& boundsColor)
			{
				if (doShowBounds)
				{
					for (auto const& boundsItr : gr::ObjectAdapter<gr::Bounds::RenderData>(renderData))
					{
						const gr::RenderDataID objectID = boundsItr->GetRenderDataID();

						if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(objectID))
						{
							if (gr::HasFeature(boundsFeatureBit, boundsItr->GetFeatureBits()))
							{
								gr::Bounds::RenderData const& boundsRenderData = boundsItr->Get<gr::Bounds::RenderData>();

								if (!boundsBatches.contains(objectID) ||
									boundsItr->IsDirty<gr::Bounds::RenderData>())
								{
									boundsBatches[objectID] = BuildBoundingBoxBatch(
										renderData, objectID, re::Lifetime::Permanent, boundsRenderData, boundsColor);
								}
								re::Batch* batch = m_debugStage->AddBatchWithLifetime(
									*boundsBatches.at(objectID), re::Lifetime::SingleFrame);

								// For simplicity, we build our lines in world space, and attach an identity transform buffer
								std::vector<gr::RenderDataID> const& associatedRenderDataIDs =
									renderData.GetRenderDataIDsReferencingTransformID(gr::k_invalidTransformID);
								SEAssert(!associatedRenderDataIDs.empty(),
									"No RenderDataIDs assocated with the parent TransformID");

								batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
										InstanceIndexData::s_shaderName, std::views::single(associatedRenderDataIDs[0])));
								batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
							}
						}
					}
				}
				else
				{
					boundsBatches.clear();
				}
			};
		
		HandleBoundsBatches( // Mesh: Draw this after MeshPrimitive bounds so they're on top if the bounding box is the same
			m_showAllMeshBounds, gr::RenderObjectFeature::IsMeshBounds, m_meshBoundsBatches, m_meshBoundsColor);

		HandleBoundsBatches(
			m_showSceneBoundingBox, gr::RenderObjectFeature::IsSceneBounds, m_sceneBoundsBatches, m_sceneBoundsColor);

		HandleBoundsBatches(
			m_showAllLightBounds, gr::RenderObjectFeature::IsLightBounds, m_lightBoundsBatches, m_lightBoundsColor);
		

		if (m_showCameraFrustums)
		{
			// TODO: BUG HERE: m_camerasToDebug may contain stale data if an object is deleted while it references it
			for (auto const& camData : m_camerasToDebug)
			{
				const gr::RenderDataID camID = camData.first;

				// Create/update a transform block for the frustum verts:
				// Use the inverse view matrix, as it omits any scale that might be present in the Transform hierarchy
				glm::mat4 const& camWorldMatrix = camData.second.first->m_cameraParams.g_invView;

				bool camDataIsDirty = renderData.IsDirty<gr::Camera::RenderData>(camID) || 
					renderData.TransformIsDirtyFromRenderDataID(camID);

				// Coordinate axis at camera origin:
				if (!m_cameraAxisBatches.contains(camID))
				{
					m_cameraAxisBatches.emplace(
						camID, 
						BuildAxisBatch(camID, re::Lifetime::Permanent));
				}
				re::Batch* batch = m_debugStage->AddBatchWithLifetime(
					*m_cameraAxisBatches.at(camID), re::Lifetime::SingleFrame);
				batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(camID)));
				batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

				// Camera frustums:
				const uint8_t numFrustums = camData.second.first->m_cameraConfig.m_projectionType ==
					gr::Camera::Config::ProjectionType::PerspectiveCubemap ? 6 : 1;

				if (!m_cameraFrustumTransformBuffers.contains(camID))
				{
					m_cameraFrustumTransformBuffers[camID].resize(numFrustums);
					camDataIsDirty = true;
				}
				if (!m_cameraFrustumBatches.contains(camID))
				{
					m_cameraFrustumBatches[camID].resize(numFrustums);
					camDataIsDirty = true;
				}

				// We're rendering lines constructed from a cube in NDC; Thus, we set the invViewProj from the camera
				// we're debugging as the world transform matrix for our cube points
				std::vector<glm::mat4> invViewProjMats;
				if (camDataIsDirty)
				{
					invViewProjMats.reserve(numFrustums);

					if (numFrustums == 6)
					{
						invViewProjMats = gr::Camera::BuildCubeInvViewProjectionMatrices(
							gr::Camera::BuildCubeViewProjectionMatrices(
								gr::Camera::BuildCubeViewMatrices(
									camData.second.second->m_globalPosition,
									camData.second.second->m_globalRight,
									camData.second.second->m_globalUp,
									camData.second.second->m_globalForward), 
								camData.second.first->m_cameraParams.g_projection));
					}
					else
					{
						invViewProjMats.emplace_back(camData.second.first->m_cameraParams.g_invViewProjection);
					}
				}
				
				for (uint8_t faceIdx = 0; faceIdx < numFrustums; faceIdx++)
				{
					if (!m_cameraFrustumTransformBuffers.at(camID)[faceIdx].IsValid())
					{
						m_cameraFrustumTransformBuffers.at(camID)[faceIdx] =
							gr::Transform::CreateTransformBufferInput(
								TransformData::s_shaderName,
								re::Lifetime::Permanent,
								re::Buffer::StagingPool::Permanent,
								&invViewProjMats.at(faceIdx),
								nullptr);
					}
					else if (camDataIsDirty)
					{
						m_cameraFrustumTransformBuffers.at(camID)[faceIdx].GetBuffer()->Commit(
							gr::Transform::CreateTransformData(&invViewProjMats.at(faceIdx), nullptr));
					}

					if (m_cameraFrustumBatches.at(camID)[faceIdx] == nullptr)
					{
						m_cameraFrustumBatches.at(camID)[faceIdx] = std::move(BuildCameraFrustumBatch(
							re::Lifetime::Permanent,
							camData.second.second,
							m_cameraFrustumColor));

						m_cameraFrustumBatches.at(camID)[faceIdx]->SetBuffer(
							m_cameraFrustumTransformBuffers.at(camID)[faceIdx]);
					}

					m_debugStage->AddBatch(*m_cameraFrustumBatches.at(camID)[faceIdx]);
				}
			}
		}
		else
		{
			m_cameraAxisBatches.clear();
			m_cameraFrustumBatches.clear();
			m_cameraFrustumTransformBuffers.clear();
		}

		if (m_showDeferredLightWireframe && 
			renderData.HasObjectData<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>())
		{
			for (auto const& pointItr : gr::ObjectAdapter<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>(renderData))
			{
				const gr::RenderDataID pointID = pointItr->GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(pointID))
				{
					if (!m_deferredLightWireframeBatches.contains(pointID))
					{
						m_deferredLightWireframeBatches.emplace(
							pointID,
							BuildWireframeBatch(
								pointID, re::Lifetime::Permanent, pointItr->Get<gr::MeshPrimitive::RenderData>()));
					}
					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_deferredLightWireframeBatches.at(pointID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(pointID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}

			auto const& spotLightMeshObjects = 
				gr::ObjectAdapter< gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>(renderData);
			for (auto const& spotItr : spotLightMeshObjects)
			{
				const gr::RenderDataID spotID = spotItr->GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(spotID))
				{
					if (!m_deferredLightWireframeBatches.contains(spotID))
					{
						m_deferredLightWireframeBatches.emplace(
							spotID,
							BuildWireframeBatch(
								spotID, re::Lifetime::Permanent, spotItr->Get<gr::MeshPrimitive::RenderData>()));
					}
					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_deferredLightWireframeBatches.at(spotID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(spotID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}
		}
		else
		{
			m_deferredLightWireframeBatches.clear();
		}

		if (m_showLightCoordinateAxis)
		{
			for (auto const& directionalItr : gr::ObjectAdapter<gr::Light::RenderDataDirectional>(renderData))
			{
				const gr::RenderDataID lightID = directionalItr->GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					m_lightCoordinateAxisBatches.emplace(
						lightID,
						BuildAxisBatch(lightID, re::Lifetime::Permanent));

					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_lightCoordinateAxisBatches.at(lightID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}

			for (auto const& pointItr : gr::ObjectAdapter<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>(renderData))
			{
				const gr::RenderDataID lightID = pointItr->GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					m_lightCoordinateAxisBatches.emplace(
						lightID,
						BuildAxisBatch(lightID, re::Lifetime::Permanent));

					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_lightCoordinateAxisBatches.at(lightID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}

			for (auto const& spotItr : gr::ObjectAdapter<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>(renderData))
			{
				const gr::RenderDataID lightID = spotItr->GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					m_lightCoordinateAxisBatches.emplace(
						lightID,
						BuildAxisBatch(lightID, re::Lifetime::Permanent));

					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_lightCoordinateAxisBatches.at(lightID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}
		}
		else
		{
			m_lightCoordinateAxisBatches.clear();
		}

		if (m_showAllTransforms)
		{
			// The IndexedBufferManager uses RenderDataIDs to resolve BufferInputs, so we must iterate over all IDs and
			// handle unique TransformIDs
			std::vector<gr::RenderDataID> const& registeredRenderDataIDs = renderData.GetRegisteredRenderDataIDs();
			std::unordered_set<gr::TransformID> seenIDs;
			seenIDs.reserve(registeredRenderDataIDs.size());
			for (gr::RenderDataID renderDataID : registeredRenderDataIDs)
			{
				const gr::TransformID transformID = renderData.GetTransformIDFromRenderDataID(renderDataID);

				if (seenIDs.contains(transformID) == false &&
					(m_selectedTransformIDs.empty() || m_selectedTransformIDs.contains(transformID)))
				{
					if (!m_transformAxisBatches.contains(transformID))
					{
						m_transformAxisBatches.emplace(
							transformID, 
							BuildAxisBatch(renderDataID, re::Lifetime::Permanent));
					}
					re::Batch* batch = m_debugStage->AddBatchWithLifetime(
						*m_transformAxisBatches.at(transformID), re::Lifetime::SingleFrame);
					batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(renderDataID)));
					batch->SetBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

					if (m_showParentChildLinks)
					{
						gr::Transform::RenderData const& transformRenderData =
							renderData.GetTransformDataFromTransformID(transformID);

						const gr::TransformID parentTransformID = transformRenderData.m_parentTransformID;
						if (parentTransformID != gr::k_invalidTransformID)
						{
							// Use the child TransformID as the key, as a node may have many children but only 1 parent
							if (!m_transformParentChildLinkBatches.contains(transformID))
							{
								m_transformParentChildLinkBatches.emplace(
									transformID, 
									BuildParentChildLinkBatch(
										renderData,
										re::Lifetime::Permanent,
										m_parentColor,
										m_childColor,
										parentTransformID,
										renderDataID));
							}
							re::Batch* batch = m_debugStage->AddBatchWithLifetime(
								*m_transformParentChildLinkBatches.at(transformID), re::Lifetime::SingleFrame);

							// Our LUT Buffers are built from RenderDataIDs; Get a list of ALL RenderDataIDs referencing the parent
							// transform, and arbitrarily use the first element to build our BufferInput
							std::vector<gr::RenderDataID> const& associatedParentRenderDataIDs =
								renderData.GetRenderDataIDsReferencingTransformID(parentTransformID);
							SEAssert(!associatedParentRenderDataIDs.empty(),
								"No RenderDataIDs assocated with the parent TransformID");

							const std::vector<gr::RenderDataID> parentChildRenderDataIDs{
								associatedParentRenderDataIDs[0], // Arbitrary: Use the 1st referencing RenderDataID
								renderDataID };

							batch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, parentChildRenderDataIDs));

							batch->SetBuffer(ibm.GetIndexedBufferInput(
								TransformData::s_shaderName, TransformData::s_shaderName));
						}
					}
				}

				seenIDs.emplace(transformID);
			}
		}
		else
		{
			m_transformAxisBatches.clear();
			m_transformParentChildLinkBatches.clear();
		}
	}


	DebugData DebugGraphicsSystem::PackDebugData() const
	{
		return DebugData{
				.g_scales = glm::vec4(
					m_vertexNormalsScale,
					m_axisScale,
					0.f,
					0.f),
				.g_colors = {
					glm::vec4(m_xAxisColor, m_axisOpacity),	// X: Red
					glm::vec4(m_yAxisColor, m_axisOpacity),	// Y: Green
					glm::vec4(m_zAxisColor, m_axisOpacity),	// Z: Blue
					m_normalsColor,
					m_wireframeColor},
		};
	}


	void DebugGraphicsSystem::ShowImGuiWindow()
	{
		constexpr ImGuiColorEditFlags k_colorPickerFlags =
			ImGuiColorEditFlags_NoInputs |
			ImGuiColorEditFlags_Float |
			ImGuiColorEditFlags_AlphaBar;


		if (ImGui::CollapsingHeader("Target render data objects"))
		{
			ImGui::Indent();

			static bool s_targetAll = true;
			if (ImGui::Button(std::format("{}", s_targetAll ? "Select specific IDs" : "Select all").c_str()))
			{
				s_targetAll = !s_targetAll;
				m_isDirty = true;
			}

			if (s_targetAll)
			{
				m_selectedRenderDataIDs.clear(); // If emtpy, render all IDs
			}
			else
			{
				std::vector<gr::RenderDataID> const& currentRenderObjects =
					m_graphicsSystemManager->GetRenderData().GetRegisteredRenderDataIDs();

				for (gr::RenderDataID renderDataID : currentRenderObjects)
				{
					const bool currentlySelected = m_selectedRenderDataIDs.contains(renderDataID);
					bool isSelected = currentlySelected;
					m_isDirty |= ImGui::Checkbox(std::format("{}", renderDataID).c_str(), &isSelected);

					if (currentlySelected && !isSelected)
					{
						m_selectedRenderDataIDs.erase(renderDataID);
					}
					else if (isSelected && !currentlySelected)
					{
						m_selectedRenderDataIDs.emplace(renderDataID);
					}
				}
			}
			ImGui::Unindent();
		}

		auto ShowColorPicker = [](bool doShow, glm::vec4& color, char const* label = nullptr) -> bool
			{
				bool isDirty = false;
				if (doShow)
				{
					ImGui::Indent();

					isDirty |= ImGui::ColorEdit4(
						std::format("{}##{}", label ? label : "Color", util::PtrToID(&color)).c_str(),
						&color.x,
						k_colorPickerFlags);

					ImGui::Unindent();
				}
				return isDirty;
			};

		m_isDirty |= ImGui::Checkbox(std::format("Show scene bounding box").c_str(), &m_showSceneBoundingBox);
		m_isDirty |= ShowColorPicker(m_showSceneBoundingBox, m_sceneBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show Mesh bounding boxes").c_str(), &m_showAllMeshBounds);
		m_isDirty |= ShowColorPicker(m_showAllMeshBounds, m_meshBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show MeshPrimitive bounding boxes").c_str(), &m_showAllMeshPrimitiveBounds);
		m_isDirty |= ShowColorPicker(m_showAllMeshPrimitiveBounds, m_meshPrimBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show Light bounding boxes").c_str(), &m_showAllLightBounds);
		m_isDirty |= ShowColorPicker(m_showAllLightBounds, m_lightBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show vertex normals").c_str(), &m_showAllVertexNormals);
		if (m_showAllVertexNormals)
		{
			ImGui::Indent();
			m_isDirty |= ImGui::ColorEdit4("Normal color", &m_normalsColor.x, ImGuiColorEditFlags_NoLabel | k_colorPickerFlags);
			ImGui::SameLine();
			m_isDirty |= ImGui::SliderFloat("Scale", &m_vertexNormalsScale, 0.f, 1.f);
			ImGui::Unindent();
		}
		
		if (ImGui::CollapsingHeader(std::format("Debug camera frustums").c_str()))
		{
			ImGui::Indent();
			m_showCameraFrustums = true;

			gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
			
			for (auto const& camItr : gr::ObjectAdapter<gr::Camera::RenderData>(renderData))
			{
				const gr::RenderDataID camID = camItr->GetRenderDataID();
				gr::Camera::RenderData const* camData = &camItr->Get<gr::Camera::RenderData>();
				gr::Transform::RenderData const* transformData = &camItr->GetTransformData();

				const bool cameraAlreadyAdded = m_camerasToDebug.contains(camID);
				bool cameraSelected = cameraAlreadyAdded;
				if (ImGui::Checkbox(
						std::format("{}##", camData->m_cameraName, util::PtrToID(camData)).c_str(), &cameraSelected) &&
					!cameraAlreadyAdded)
				{
					m_camerasToDebug.emplace(camID, std::make_pair(camData, transformData));
					m_isDirty = true;
				}
				else if (cameraAlreadyAdded && !cameraSelected)
				{
					m_camerasToDebug.erase(camID);
				}
			}
			ImGui::Unindent();
		}
		else
		{
			m_showCameraFrustums = false;
			m_camerasToDebug.clear();
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show origin coordinate XYZ axis").c_str(), &m_showWorldCoordinateAxis);
		m_isDirty |= ImGui::Checkbox(std::format("Show mesh coordinate axis").c_str(), &m_showMeshCoordinateAxis);
		m_isDirty |= ImGui::Checkbox(std::format("Show light coordinate axis").c_str(), &m_showLightCoordinateAxis);

		m_isDirty |= ImGui::Checkbox(std::format("Show all transform axes").c_str(), &m_showAllTransforms);
		if (m_showAllTransforms)
		{
			ImGui::Indent();

			if (ImGui::CollapsingHeader("Target TransformIDs"))
			{
				ImGui::Indent();

				static bool s_targetAll = true;
				if (ImGui::Button(std::format("{}", s_targetAll ? "Select specific IDs" : "Select all").c_str()))
				{
					s_targetAll = !s_targetAll;
					m_isDirty = true;
				}

				if (s_targetAll)
				{
					m_selectedTransformIDs.clear(); // If emtpy, render all IDs
				}
				else
				{
					std::vector<gr::RenderDataID> const& currentTransforms =
						m_graphicsSystemManager->GetRenderData().GetRegisteredTransformIDs();

					for (gr::RenderDataID transformID : currentTransforms)
					{
						const bool currentlySelected = m_selectedTransformIDs.contains(transformID);
						bool isSelected = currentlySelected;
						m_isDirty |= ImGui::Checkbox(std::format("{}", transformID).c_str(), &isSelected);

						if (currentlySelected && !isSelected)
						{
							m_selectedTransformIDs.erase(transformID);
						}
						else if (isSelected && !currentlySelected)
						{
							m_selectedTransformIDs.emplace(transformID);
						}
					}
				}
				ImGui::Unindent();
			}

			m_isDirty |= ImGui::Checkbox(std::format("Show parent/child links").c_str(), &m_showParentChildLinks);
			ShowColorPicker(m_showParentChildLinks, m_parentColor, "Parent");
			ShowColorPicker(m_showParentChildLinks, m_childColor, "Child");
			ImGui::Unindent();
		}

		if (m_showWorldCoordinateAxis || 
			m_showMeshCoordinateAxis || 
			m_showLightCoordinateAxis || 
			m_showCameraFrustums || 
			m_showAllTransforms)
		{
			ImGui::Indent();
			m_isDirty |= ImGui::SliderFloat("Axis scale", &m_axisScale, 0.f, 1.f);
			m_isDirty |= ImGui::SliderFloat("Axis opacity", &m_axisOpacity, 0.f, 1.f);
			ImGui::Unindent();
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show mesh wireframes").c_str(), &m_showAllWireframe);
		m_isDirty |= ImGui::Checkbox(std::format("Show deferred light mesh wireframes").c_str(), &m_showDeferredLightWireframe);
		m_isDirty |= ShowColorPicker(m_showAllWireframe || m_showDeferredLightWireframe, m_wireframeColor);
	}
}