// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BatchBuilder.h"
#include "BoundsRenderData.h"
#include "BufferView.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "LightRenderData.h"
#include "TransformRenderData.h"

#include "Core/InvPtr.h"
#include "Core/SystemLocator.h"

#include "Core/Util/ByteVector.h"
#include "Core/Util/ImGuiUtils.h"

#include "Renderer/Shaders/Common/DebugParams.h"
#include "Renderer/Shaders/Common/InstancingParams.h"
#include "Renderer/Shaders/Common/TransformParams.h"


namespace
{
	static const EffectID k_debugEffectID = effect::Effect::ComputeEffectID("Debug");


	gr::BatchHandle BuildAxisBatch()
	{
		util::ByteVector axisOriginPos = util::ByteVector::Create<glm::vec3>({ glm::vec3(0.f, 0.f, 0.f) });

		core::InvPtr<re::VertexStream> const& axisPositionStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(axisOriginPos));

		gr::BatchHandle batch = gr::RasterBatchBuilder()
			.SetGeometryMode(re::Batch::GeometryMode::ArrayInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::PointList)
			.SetVertexBuffer(0, axisPositionStream)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_Axis)
			.Build();
		
		return batch;
	}


	gr::BatchHandle BuildParentChildLinkBatch(
		glm::vec4 const& parentColor,
		glm::vec4 const& childColor)
	{
		constexpr glm::vec4 k_originPoint(0.f, 0.f, 0.f, 1.f);

		core::InvPtr<re::VertexStream> const& linePositionsStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			util::ByteVector::Create<glm::vec3>({ k_originPoint, k_originPoint })); // [0] = parent, [1] = child
			

		core::InvPtr<re::VertexStream> const& lineColorStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			util::ByteVector::Create<glm::vec4>({ parentColor, childColor }));

		core::InvPtr<re::VertexStream> const& lineIndexStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			util::ByteVector::Create<uint16_t>({ 0, 1 }));

		gr::BatchHandle batch = gr::RasterBatchBuilder()
			.SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::LineList)
			.SetVertexBuffers({ linePositionsStream, lineColorStream })
			.SetIndexBuffer(lineIndexStream)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_VertexIDInstancingLUTIdx)
			.Build();

		return batch;
	}


	gr::BatchHandle BuildBoundingBoxBatch(gr::Bounds::RenderData const& bounds, glm::vec4 const& boxColor)
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

		core::InvPtr<re::VertexStream> const& boxPositionsStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(boxPositions));

		core::InvPtr<re::VertexStream> const& boxColorStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(boxColors));

		core::InvPtr<re::VertexStream> const& boxIndexStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			std::move(boxIndexes));

		gr::BatchHandle batch = gr::RasterBatchBuilder()
			.SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::LineList)
			.SetVertexBuffer(0, boxPositionsStream)
			.SetVertexBuffer(1, boxColorStream)
			.SetIndexBuffer(boxIndexStream)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_Line)
			.Build();

		return batch;
	}


	gr::BatchHandle BuildVertexNormalsBatch(gr::BatchHandle existingBatch)
	{
		if ((*existingBatch).GetRasterParams().HasVertexStream(re::VertexStream::Type::Normal) == false)
		{
			return gr::BatchHandle(); // No normals? Nothing to build
		}

		SEAssert((*existingBatch).GetRasterParams().HasVertexStream(re::VertexStream::Position),
			"Existing Batch has no Position vertex stream. This should not be possible");

		re::Batch::RasterParams const& rasterParams = (*existingBatch).GetRasterParams();

		SEAssert(
			rasterParams.GetVertexStreamInput(re::VertexStream::Position)->GetStream()->GetDataType() == re::DataType::Float3 &&
			rasterParams.GetVertexStreamInput(re::VertexStream::Normal)->GetStream()->GetDataType() == re::DataType::Float3,
			"Unexpected position or normal data");

		gr::BatchHandle batch = gr::RasterBatchBuilder::CloneAndModify(existingBatch)
			.SetGeometryMode(re::Batch::GeometryMode::ArrayInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::PointList)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_Normal)
			.Build();

		return batch;
	}

	
	gr::BatchHandle BuildCameraFrustumBatch(
		glm::vec4 const& frustumColor,
		re::BufferInput const& camFrustumTransformBuffer)
	{
		// NDC coordinates:
		glm::vec4 farTL(-1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBL(-1.f, -1.f, 1.f, 1.f);
		glm::vec4 farTR(1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBR(1.f, -1.f, 1.f, 1.f);
		glm::vec4 nearTL(-1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBL(-1.f, -1.f, 0.f, 1.f);
		glm::vec4 nearTR(1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBR(1.f, -1.f, 0.f, 1.f);

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

		core::InvPtr<re::VertexStream> const& frustumPositionsStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(frustumPositions));

		core::InvPtr<re::VertexStream> const& frustumColorStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(frustumColors));

		core::InvPtr<re::VertexStream> const& frustumIndexStream = re::VertexStream::Create(
			re::VertexStream::StreamDesc{
				.m_type = re::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			std::move(frustumIndexes));

		gr::BatchHandle batch = gr::RasterBatchBuilder()
			.SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::LineList)
			.SetVertexBuffer(0, frustumPositionsStream)
			.SetVertexBuffer(1, frustumColorStream)
			.SetIndexBuffer(frustumIndexStream)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_InstanceIDTransformIdx)
			.SetBuffer(camFrustumTransformBuffer)
			.Build();

		return batch;
	}
	

	gr::BatchHandle BuildWireframeBatch(gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		core::InvPtr<re::VertexStream> const& positionStream = 
			gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
				meshPrimRenderData,
				re::VertexStream::Type::Position);

		core::InvPtr<re::VertexStream> const& indexStream = meshPrimRenderData.m_indexStream;
		SEAssert(positionStream && indexStream, "Must have a position and index stream");

		gr::BatchHandle batch = gr::RasterBatchBuilder()
			.SetGeometryMode(re::Batch::GeometryMode::IndexedInstanced)
			.SetPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology::TriangleList)
			.SetVertexBuffer(0, positionStream)
			.SetIndexBuffer(indexStream)
			.SetEffectID(k_debugEffectID)
			.SetDrawstyleBitmask(effect::drawstyle::Debug_Wireframe)
			.Build();

		return batch;
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
		core::SystemLocator::Register<gr::DebugGraphicsSystem>(ACCESS_KEY(AccessKey), this);
	}


	DebugGraphicsSystem::~DebugGraphicsSystem()
	{
		core::SystemLocator::Unregister<gr::DebugGraphicsSystem>(ACCESS_KEY(AccessKey));
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

		// We'll set our transform buffers manually, disable instancing so they don't get stomped
		m_debugStage->SetInstancingEnabled(false);

		stagePipeline.AppendStage(m_debugStage);


		m_wireframeStage = re::Stage::CreateGraphicsStage("Debug: Wireframe stage", re::Stage::GraphicsStageParams{});

		m_wireframeStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer
		m_wireframeStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_wireframeStage->AddPermanentBuffer(m_debugParams);
		m_wireframeStage->AddDrawStyleBits(effect::drawstyle::Debug_Wireframe);

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

		const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
		SEAssert(mainCamID == gr::k_invalidRenderDataID ||
			m_viewBatches->contains(mainCamID),
			"Cannot find main camera ID in view batches");

		if (m_serviceData.m_showWorldCoordinateAxis)
		{
			// Use the 1st RenderDataID that references the identity transform to obtain a view
			std::vector<gr::RenderDataID> const& identityObjects =
				renderData.GetRenderDataIDsReferencingTransformID(k_invalidTransformID);
			SEAssert(!identityObjects.empty(), "No RenderDataIDs associated with the identity transform");

			if (m_worldCoordinateAxisBatch.IsValid() == false)
			{
				m_worldCoordinateAxisBatch = BuildAxisBatch();
			}
			gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_worldCoordinateAxisBatch);

			batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
				InstanceIndexData::s_shaderName, std::views::single(identityObjects[0])));
			batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
		}
		else
		{
			m_worldCoordinateAxisBatch = BatchHandle();
		}

		if (m_showAllWireframe && mainCamID != gr::k_invalidRenderDataID)
		{
			std::vector<gr::BatchHandle> const& mainCamBatches = m_viewBatches->at(mainCamID);
			for (gr::BatchHandle const& batch : mainCamBatches)
			{
				m_wireframeStage->AddBatch(batch);
			}
		}

		if (m_showAllVertexNormals && mainCamID != gr::k_invalidRenderDataID)
		{
			std::vector<gr::BatchHandle> const& mainCamBatches = m_viewBatches->at(mainCamID);
			for (gr::BatchHandle const& batch : mainCamBatches)
			{
				const gr::RenderDataID batchRenderDataID = batch.GetRenderDataID();
				SEAssert(batchRenderDataID != gr::k_invalidRenderDataID,
					"Found a main camera batch with an invalid RenderDataID");

				if (!m_vertexNormalBatches.contains(batchRenderDataID))
				{
					const gr::BatchHandle normalsBatch = BuildVertexNormalsBatch(batch);
					if (normalsBatch.IsValid())
					{
						m_vertexNormalBatches.emplace(batchRenderDataID, normalsBatch);
					}
				}
				gr::StageBatchHandle& batch =
					*m_debugStage->AddBatch(m_vertexNormalBatches.at(batchRenderDataID));
				batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
					InstanceIndexData::s_shaderName, std::views::single(batchRenderDataID)));
				batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
			}
		}
		else
		{
			m_vertexNormalBatches.clear();
		}

		if (m_showAllMeshPrimitiveBounds || 
			m_showMeshCoordinateAxis)
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
									BuildBoundingBoxBatch(boundsRenderData, m_meshPrimBoundsColor);
							}
							gr::StageBatchHandle& batch = 
								*m_debugStage->AddBatch(m_meshPrimBoundsBatches.at(meshPrimRenderDataID));

							// For simplicity, we build our lines in world space, and attach an identity transform buffer
							std::vector<gr::RenderDataID> const& associatedRenderDataIDs =
								renderData.GetRenderDataIDsReferencingTransformID(gr::k_invalidTransformID);
							SEAssert(!associatedRenderDataIDs.empty(),
								"No RenderDataIDs assocated with the parent TransformID");

							batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
									InstanceIndexData::s_shaderName, std::views::single(associatedRenderDataIDs[0])));
							batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
						}
					}

					if (m_showMeshCoordinateAxis)
					{
						if (!m_meshCoordinateAxisBatches.contains(meshPrimRenderDataID))
						{
							m_meshCoordinateAxisBatches.emplace(meshPrimRenderDataID, BuildAxisBatch());
						}
						gr::StageBatchHandle& batch = 
							*m_debugStage->AddBatch(m_meshCoordinateAxisBatches.at(meshPrimRenderDataID));
						batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
							InstanceIndexData::s_shaderName, std::views::single(meshPrimRenderDataID)));
						batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
					}
				}
			}
		}
		else
		{
			m_meshPrimBoundsBatches.clear();
			m_meshCoordinateAxisBatches.clear();
		}

		auto HandleBoundsBatches = [this, &renderData, &ibm](
			bool doShowBounds,
			gr::RenderObjectFeature boundsFeatureBit, 
			std::unordered_map<gr::RenderDataID, gr::BatchHandle>& boundsBatches,
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
									boundsBatches[objectID] = BuildBoundingBoxBatch(boundsRenderData, boundsColor);
								}
								gr::StageBatchHandle& batch = *m_debugStage->AddBatch(boundsBatches.at(objectID));

								// For simplicity, we build our lines in world space, and attach an identity transform buffer
								std::vector<gr::RenderDataID> const& associatedRenderDataIDs =
									renderData.GetRenderDataIDsReferencingTransformID(gr::k_invalidTransformID);
								SEAssert(!associatedRenderDataIDs.empty(),
									"No RenderDataIDs assocated with the parent TransformID");

								batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
										InstanceIndexData::s_shaderName, std::views::single(associatedRenderDataIDs[0])));
								batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
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
					m_cameraAxisBatches.emplace(camID, BuildAxisBatch());
				}
				gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_cameraAxisBatches.at(camID));
				batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(camID)));
				batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

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

					if (m_cameraFrustumBatches.at(camID)[faceIdx].IsValid() == false)
					{
						m_cameraFrustumBatches.at(camID)[faceIdx] = BuildCameraFrustumBatch(
							m_cameraFrustumColor,
							m_cameraFrustumTransformBuffers.at(camID)[faceIdx]);
					}

					m_debugStage->AddBatch(m_cameraFrustumBatches.at(camID)[faceIdx]);
				}
			}
		}
		else
		{
			m_cameraAxisBatches.clear();
			m_cameraFrustumBatches.clear();
			m_cameraFrustumTransformBuffers.clear();
		}

		if (m_showDeferredLightWireframe)
		{
			if (renderData.HasObjectData<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>())
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
								BuildWireframeBatch(pointItr->Get<gr::MeshPrimitive::RenderData>()));
						}
						gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_deferredLightWireframeBatches.at(pointID));
						batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
							InstanceIndexData::s_shaderName, std::views::single(pointID)));
						batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
					}
				}
			}

			if (renderData.HasObjectData<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>())
			{
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
								BuildWireframeBatch(spotItr->Get<gr::MeshPrimitive::RenderData>()));
						}
						gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_deferredLightWireframeBatches.at(spotID));
						batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
							InstanceIndexData::s_shaderName, std::views::single(spotID)));
						batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
					}
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
					m_lightCoordinateAxisBatches.emplace(lightID, BuildAxisBatch());

					gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_lightCoordinateAxisBatches.at(lightID));
					batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}

			for (auto const& pointItr : gr::ObjectAdapter<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>(renderData))
			{
				const gr::RenderDataID lightID = pointItr->GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					m_lightCoordinateAxisBatches.emplace(lightID, BuildAxisBatch());

					gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_lightCoordinateAxisBatches.at(lightID));
					batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
				}
			}

			for (auto const& spotItr : gr::ObjectAdapter<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>(renderData))
			{
				const gr::RenderDataID lightID = spotItr->GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					m_lightCoordinateAxisBatches.emplace(lightID, BuildAxisBatch());

					gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_lightCoordinateAxisBatches.at(lightID));
					batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(lightID)));
					batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));
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
						m_transformAxisBatches.emplace(transformID, BuildAxisBatch());
					}
					gr::StageBatchHandle& batch = *m_debugStage->AddBatch(m_transformAxisBatches.at(transformID));
					batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
						InstanceIndexData::s_shaderName, std::views::single(renderDataID)));
					batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

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
										m_parentColor,
										m_childColor));
							}
							gr::StageBatchHandle& batch = 
								*m_debugStage->AddBatch(m_transformParentChildLinkBatches.at(transformID));

							// Our LUT Buffers are built from RenderDataIDs; Get a list of ALL RenderDataIDs referencing the parent
							// transform, and arbitrarily use the first element to build our BufferInput
							std::vector<gr::RenderDataID> const& associatedParentRenderDataIDs =
								renderData.GetRenderDataIDsReferencingTransformID(parentTransformID);
							SEAssert(!associatedParentRenderDataIDs.empty(),
								"No RenderDataIDs assocated with the parent TransformID");

							const std::vector<gr::RenderDataID> parentChildRenderDataIDs{
								associatedParentRenderDataIDs[0], // Arbitrary: Use the 1st referencing RenderDataID
								renderDataID };

							batch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, parentChildRenderDataIDs));

							batch.SetSingleFrameBuffer(ibm.GetIndexedBufferInput(
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
					m_serviceData.m_axisScale,
					0.f,
					0.f),
				.g_colors = {
					glm::vec4(m_serviceData.m_xAxisColor, m_serviceData.m_axisOpacity),	// X: Red
					glm::vec4(m_serviceData.m_yAxisColor, m_serviceData.m_axisOpacity),	// Y: Green
					glm::vec4(m_serviceData.m_zAxisColor, m_serviceData.m_axisOpacity),	// Z: Blue
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

		if (m_serviceData.m_showWorldCoordinateAxis ||
			m_showMeshCoordinateAxis || 
			m_showLightCoordinateAxis || 
			m_showCameraFrustums || 
			m_showAllTransforms)
		{
			ImGui::Indent();
			m_isDirty |= ImGui::SliderFloat("Axis scale", &m_serviceData.m_axisScale, 0.f, 1.f);
			m_isDirty |= ImGui::SliderFloat("Axis opacity", &m_serviceData.m_axisOpacity, 0.f, 1.f);
			ImGui::Unindent();
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show mesh wireframes").c_str(), &m_showAllWireframe);
		m_isDirty |= ImGui::Checkbox(std::format("Show deferred light mesh wireframes").c_str(), &m_showDeferredLightWireframe);
		m_isDirty |= ShowColorPicker(m_showAllWireframe || m_showDeferredLightWireframe, m_wireframeColor);
	}


	void DebugGraphicsSystem::EnableWorldCoordinateAxis(AccessKey, bool show)
	{
		m_isDirty |= (show != m_serviceData.m_showWorldCoordinateAxis);
		m_serviceData.m_showWorldCoordinateAxis = show;
	}
}