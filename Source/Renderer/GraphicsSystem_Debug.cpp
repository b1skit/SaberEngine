// © 2023 Adam Badke. All rights reserved.
#include "Batch.h"
#include "BoundsRenderData.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "TransformRenderData.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Util/ByteVector.h"
#include "Core/Util/ImGuiUtils.h"

#include "Shaders/Common/DebugParams.h"


namespace
{
	static constexpr char const* k_debugEffectName = "Debug";

	static const EffectID k_debugEffectID = effect::Effect::ComputeEffectID(k_debugEffectName);


	std::unique_ptr<re::Batch> BuildAxisBatch(
		re::Lifetime batchLifetime,
		float axisScale,
		glm::vec3 const& xAxisColor,
		glm::vec3 const& yAxisColor,
		glm::vec3 const& zAxisColor,
		float axisOpacity)
	{
		util::ByteVector axisPositions = util::ByteVector::Create<glm::vec3>({
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisX * axisScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisY * axisScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisZ * axisScale,
		});

		util::ByteVector axisColors = util::ByteVector::Create<glm::vec4>({
			glm::vec4(xAxisColor, axisOpacity), glm::vec4(xAxisColor, axisOpacity),
			glm::vec4(yAxisColor, axisOpacity), glm::vec4(yAxisColor, axisOpacity),
			glm::vec4(zAxisColor, axisOpacity), glm::vec4(zAxisColor, axisOpacity),
		});

		const re::Lifetime streamLifetime = batchLifetime;

		std::shared_ptr<gr::VertexStream> axisPositionStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(axisPositions),
			false);

		std::shared_ptr<gr::VertexStream> axisColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(axisColors),
			false);

		re::Batch::GraphicsParams axisBatchGraphicsParams{};
		axisBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		axisBatchGraphicsParams.m_numInstances = 1;
		axisBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::LineList;

		axisBatchGraphicsParams.m_vertexBuffers[0] = re::VertexBufferInput(axisPositionStream.get());
		axisBatchGraphicsParams.m_vertexBuffers[1] = re::VertexBufferInput(axisColorStream.get());

		std::unique_ptr<re::Batch> axisBatch = std::make_unique<re::Batch>(
			batchLifetime, axisBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Line);

		return axisBatch;
	}


	std::unique_ptr<re::Batch> BuildBoundingBoxBatch(
		re::Lifetime batchLifetime, gr::Bounds::RenderData const& bounds, glm::vec4 const& boxColor)
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
		const float xMin = bounds.m_minXYZ.x;
		const float yMin = bounds.m_minXYZ.y;
		const float zMin = bounds.m_minXYZ.z;

		const float xMax = bounds.m_maxXYZ.x;
		const float yMax = bounds.m_maxXYZ.y;
		const float zMax = bounds.m_maxXYZ.z;

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

		const re::Lifetime streamLifetime = batchLifetime;

		std::shared_ptr<gr::VertexStream> boxPositionsStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(boxPositions),
			false);

		std::shared_ptr<gr::VertexStream> boxColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(boxColors),
			false);

		std::shared_ptr<gr::VertexStream> boxIndexStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Index,
				.m_dataType = re::DataType::UShort,
			},
			std::move(boxIndexes),
			false);

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::LineList;

		boundingBoxBatchGraphicsParams.m_vertexBuffers[0] = boxPositionsStream.get();
		boundingBoxBatchGraphicsParams.m_vertexBuffers[1] = boxColorStream.get();

		boundingBoxBatchGraphicsParams.m_indexBuffer = re::VertexBufferInput(boxIndexStream.get());

		std::unique_ptr<re::Batch> boundingBoxBatch = std::make_unique<re::Batch>(
			batchLifetime, boundingBoxBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Line);

		return boundingBoxBatch;
	}


	std::unique_ptr<re::Batch> BuildVertexNormalsBatch(
		re::Lifetime batchLifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData)
	{
		gr::VertexStream const* normalStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
				meshPrimRenderData, gr::VertexStream::Type::Normal);
		if (normalStream == nullptr)
		{
			return nullptr; // No normals? Nothing to build
		}

		gr::VertexStream const* positionStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
			meshPrimRenderData, gr::VertexStream::Type::Position);
		SEAssert(positionStream, "Cannot find position stream");

		SEAssert(positionStream->GetDataType() == re::DataType::Float3 && 
			normalStream->GetDataType() == re::DataType::Float3,
			"Unexpected position or normal data");

		const re::Batch::GraphicsParams normalBatchGraphicsParams{
			.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced,
			.m_numInstances = 1,
			.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::PointList,
			.m_vertexBuffers = { positionStream, normalStream, },
		};

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

		std::shared_ptr<gr::VertexStream> frustumPositionsStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Position,
				.m_dataType = re::DataType::Float3,
			},
			std::move(frustumPositions),
			false);

		std::shared_ptr<gr::VertexStream> frustumColorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(frustumColors),
			false);

		std::shared_ptr<gr::VertexStream> frustumIndexStream = gr::VertexStream::Create(
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

		frustumBatchGraphicsParams.m_vertexBuffers[0] = frustumPositionsStream.get();
		frustumBatchGraphicsParams.m_vertexBuffers[1] = frustumColorStream.get();

		frustumBatchGraphicsParams.m_indexBuffer = frustumIndexStream.get();

		std::unique_ptr<re::Batch> frustumBatch = std::make_unique<re::Batch>(
			batchLifetime, frustumBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Line);

		return frustumBatch;
	}
	

	std::unique_ptr<re::Batch> BuildWireframeBatch(
		re::Lifetime batchLifetime, 
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		glm::vec4 const& meshColor)
	{
		gr::VertexStream const* positionStream = gr::MeshPrimitive::RenderData::GetVertexStreamFromRenderData(
			meshPrimRenderData, gr::VertexStream::Type::Position);

		gr::VertexStream const* indexStream = meshPrimRenderData.m_indexStream;
		SEAssert(positionStream && indexStream, "Must have a position and index stream");

		util::ByteVector meshColors = util::ByteVector::Create<glm::vec4>(positionStream->GetNumElements(), meshColor);

		const re::Lifetime streamLifetime = batchLifetime;

		std::shared_ptr<gr::VertexStream> colorStream = gr::VertexStream::Create(
			gr::VertexStream::StreamDesc{
				.m_lifetime = streamLifetime,
				.m_type = gr::VertexStream::Type::Color,
				.m_dataType = re::DataType::Float4
			},
			std::move(meshColors),
			false);

		re::Batch::GraphicsParams wireframeBatchGraphicsParams{};
		wireframeBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		wireframeBatchGraphicsParams.m_numInstances = 1;
		wireframeBatchGraphicsParams.m_primitiveTopology = gr::MeshPrimitive::PrimitiveTopology::TriangleList;

		wireframeBatchGraphicsParams.m_vertexBuffers[0] = positionStream;
		wireframeBatchGraphicsParams.m_vertexBuffers[1] = colorStream;

		wireframeBatchGraphicsParams.m_indexBuffer = indexStream;

		std::unique_ptr<re::Batch> wireframeBatch = std::make_unique<re::Batch>(
			batchLifetime, wireframeBatchGraphicsParams, k_debugEffectID, effect::drawstyle::Debug_Triangle);

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
	DebugGraphicsSystem::DebugGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_isDirty(true)
	{
	}


	void DebugGraphicsSystem::InitPipeline(
		re::StagePipeline& stagePipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
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

		// Line topology stage:
		m_debugLineStage = 
			re::RenderStage::CreateGraphicsStage("Debug line stage", re::RenderStage::GraphicsStageParams{});
		
		m_debugLineStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer
		m_debugLineStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_debugLineStage->AddPermanentBuffer(m_debugParams);

		stagePipeline.AppendRenderStage(m_debugLineStage);
		
		// Triangle topology stage:
		m_debugTriangleStage =
			re::RenderStage::CreateGraphicsStage("Debug triangle stage", re::RenderStage::GraphicsStageParams{});

		m_debugTriangleStage->SetTextureTargetSet(nullptr);
		m_debugTriangleStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_debugTriangleStage->AddPermanentBuffer(m_debugParams);

		stagePipeline.AppendRenderStage(m_debugTriangleStage);

		
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

		if (m_showWorldCoordinateAxis)
		{
			if (m_worldCoordinateAxisBatch == nullptr)
			{
				m_worldCoordinateAxisBatch = std::move(BuildAxisBatch(re::Lifetime::Permanent, 
						m_worldCoordinateAxisScale, 
						m_xAxisColor, 
						m_yAxisColor, 
						m_zAxisColor,
						m_axisOpacity));

				re::BufferInput const& identityTransformBuffer = gr::Transform::CreateInstancedTransformBuffer(
					re::Lifetime::Permanent, re::Buffer::StagingPool::Temporary, &k_identity, nullptr);

				m_worldCoordinateAxisBatch->SetBuffer(identityTransformBuffer);
			}

			m_debugLineStage->AddBatch(*m_worldCoordinateAxisBatch);
		}
		else
		{
			m_worldCoordinateAxisBatch = nullptr;
		}

		if (m_showAllMeshPrimitiveBoundingBoxes || 
			m_showMeshCoordinateAxis || 
			m_showAllVertexNormals ||
			m_showAllWireframe)
		{

			auto meshPrimItr = renderData.ObjectBegin<gr::MeshPrimitive::RenderData, gr::Bounds::RenderData>();
			auto const& meshPrimItrEnd = renderData.ObjectEnd<gr::MeshPrimitive::RenderData, gr::Bounds::RenderData>();
			while (meshPrimItr != meshPrimItrEnd)
			{
				// Skip deferred light meshes
				if (!gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitive, meshPrimItr.GetFeatureBits()))
				{
					++meshPrimItr;
					continue;
				}

				const gr::RenderDataID meshPrimRenderDataID = meshPrimItr.GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(meshPrimRenderDataID))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData = 
						meshPrimItr.Get<gr::MeshPrimitive::RenderData>();
					gr::Bounds::RenderData const& boundsRenderData = meshPrimItr.Get<gr::Bounds::RenderData>();

					gr::Transform::RenderData const& transformData = meshPrimItr.GetTransformData();

					/*// Adjust the scale of the mesh's global TRS matrix basis vectors:
					glm::mat4 const& meshTR = 
						AdjustMat4Scale(transformData.g_model, transformData.m_globalScale, m_meshCoordinateAxisScale);*/

					// Create/update a cached buffer:
					if (!m_meshPrimTransformBuffers.contains(meshPrimRenderDataID))
					{
						m_meshPrimTransformBuffers.emplace(
							meshPrimRenderDataID, 
							gr::Transform::CreateInstancedTransformBuffer(
								re::Lifetime::Permanent,
								re::Buffer::StagingPool::Permanent, 
								transformData));
					}
					else
					{
						m_meshPrimTransformBuffers.at(meshPrimRenderDataID).GetBuffer()->Commit(
							gr::Transform::CreateInstancedTransformData(
								&transformData.g_model, &transformData.g_transposeInvModel));
					}
					re::BufferInput const& meshTransformBuffer = 
						m_meshPrimTransformBuffers.at(meshPrimRenderDataID);

					// MeshPrimitives:
					if (m_showAllMeshPrimitiveBoundingBoxes || m_showAllVertexNormals || m_showAllWireframe)
					{
						if (m_showAllMeshPrimitiveBoundingBoxes && 
							gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveBounds, meshPrimItr.GetFeatureBits()))
						{
							if (!m_meshPrimBoundingBoxBatches.contains(meshPrimRenderDataID))
							{
								m_meshPrimBoundingBoxBatches.emplace(
									meshPrimRenderDataID,
									BuildBoundingBoxBatch(
										re::Lifetime::Permanent, boundsRenderData, m_meshPrimitiveBoundsColor));

								m_meshPrimBoundingBoxBatches.at(meshPrimRenderDataID)->SetBuffer(meshTransformBuffer);
							}
							m_debugLineStage->AddBatch(*m_meshPrimBoundingBoxBatches.at(meshPrimRenderDataID));
						}

						if (m_showAllVertexNormals)
						{
							if (!m_vertexNormalBatches.contains(meshPrimRenderDataID))
							{
								std::unique_ptr<re::Batch> normalsBatch = 
									BuildVertexNormalsBatch(re::Lifetime::Permanent, meshPrimRenderData);

								if (normalsBatch)
								{
									m_vertexNormalBatches.emplace(
										meshPrimRenderDataID,
										std::move(normalsBatch));

									m_vertexNormalBatches.at(meshPrimRenderDataID)->SetBuffer(meshTransformBuffer);
								}
							}
							m_debugLineStage->AddBatch(*m_vertexNormalBatches.at(meshPrimRenderDataID));
						}

						if (m_showAllWireframe)
						{
							if (!m_wireframeBatches.contains(meshPrimRenderDataID))
							{
								m_wireframeBatches.emplace(
									meshPrimRenderDataID,
									BuildWireframeBatch(
										re::Lifetime::Permanent, meshPrimRenderData, m_wireframeColor));

								m_wireframeBatches.at(meshPrimRenderDataID)->SetBuffer(meshTransformBuffer);
							}
							m_debugTriangleStage->AddBatch(*m_wireframeBatches.at(meshPrimRenderDataID));
						}
					}

					if (m_showMeshCoordinateAxis)
					{
						if (!m_meshCoordinateAxisBatches.contains(meshPrimRenderDataID))
						{
							m_meshCoordinateAxisBatches.emplace(
								meshPrimRenderDataID, 
								std::move(BuildAxisBatch(
									re::Lifetime::Permanent,
									m_meshCoordinateAxisScale,
									m_xAxisColor,
									m_yAxisColor,
									m_zAxisColor,
									m_axisOpacity)));

							m_meshCoordinateAxisBatches.at(meshPrimRenderDataID)->SetBuffer(meshTransformBuffer);
						}

						m_debugLineStage->AddBatch(*m_meshCoordinateAxisBatches.at(meshPrimRenderDataID));
					}
				}
				++meshPrimItr;
			}
		}
		else
		{
			m_meshPrimTransformBuffers.clear();

			m_meshPrimBoundingBoxBatches.clear();
			m_vertexNormalBatches.clear();
			m_wireframeBatches.clear();
			m_meshCoordinateAxisBatches.clear();
		}

		// Mesh: Draw this after MeshPrimitive bounds so they're on top if the bounding box is the same
		if (m_showAllMeshBoundingBoxes)
		{
			auto boundsItr = renderData.ObjectBegin<gr::Bounds::RenderData>();
			auto boundsItrEnd = renderData.ObjectEnd<gr::Bounds::RenderData>();
			while (boundsItr != boundsItrEnd)
			{
				const gr::RenderDataID meshID = boundsItr.GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(meshID))
				{
					if (gr::HasFeature(gr::RenderObjectFeature::IsMeshBounds, boundsItr.GetFeatureBits()))
					{
						gr::Bounds::RenderData const& boundsRenderData = boundsItr.Get<gr::Bounds::RenderData>();

						if (!m_meshBoundingBoxBuffers.contains(meshID))
						{
							m_meshBoundingBoxBuffers.emplace(
								meshID,
								gr::Transform::CreateInstancedTransformBuffer(
									re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, boundsItr.GetTransformData()));
						}
						else
						{
							m_meshBoundingBoxBuffers.at(meshID).GetBuffer()->Commit(
								gr::Transform::CreateInstancedTransformData(boundsItr.GetTransformData()));
						}

						if (!m_meshBoundingBoxBatches.contains(meshID))
						{
							m_meshBoundingBoxBatches.emplace(meshID,
								BuildBoundingBoxBatch(re::Lifetime::Permanent,boundsRenderData, m_meshBoundsColor));

							m_meshBoundingBoxBatches.at(meshID)->SetBuffer(
								m_meshBoundingBoxBuffers.at(meshID));
						}

						m_debugLineStage->AddBatch(*m_meshBoundingBoxBatches.at(meshID));
					}
				}
				++boundsItr;
			}
		}
		else
		{
			m_meshBoundingBoxBatches.clear();
		}

		// Scene bounds
		if (m_showSceneBoundingBox)
		{
			auto boundsItr = renderData.ObjectBegin<gr::Bounds::RenderData>();
			auto boundsItrEnd = renderData.ObjectEnd<gr::Bounds::RenderData>();
			while (boundsItr != boundsItrEnd)
			{
				if (gr::HasFeature(gr::RenderObjectFeature::IsSceneBounds, boundsItr.GetFeatureBits()))
				{
					gr::Bounds::RenderData const& boundsRenderData = boundsItr.Get<gr::Bounds::RenderData>();

					if (!m_sceneBoundsTransformBuffer.IsValid())
					{
						m_sceneBoundsTransformBuffer = gr::Transform::CreateInstancedTransformBuffer(
							re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, boundsItr.GetTransformData());
					}

					if (m_sceneBoundsBatch == nullptr)
					{
						m_sceneBoundsBatch =
							BuildBoundingBoxBatch(re::Lifetime::Permanent, boundsRenderData, m_sceneBoundsColor);

						m_sceneBoundsBatch->SetBuffer(m_sceneBoundsTransformBuffer);
					}
					
					m_debugLineStage->AddBatch(*m_sceneBoundsBatch);
				}
				++boundsItr;
			}
		}
		else
		{
			m_sceneBoundsBatch = nullptr;
			m_sceneBoundsTransformBuffer.Release();
		}

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

				if (!m_cameraAxisTransformBuffers.contains(camID))
				{	
					m_cameraAxisTransformBuffers.emplace(
						camID,
						gr::Transform::CreateInstancedTransformBuffer(
							re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, &camWorldMatrix, nullptr));
				}
				else if (camDataIsDirty)
				{
					m_cameraAxisTransformBuffers.at(camID).GetBuffer()->Commit(
						gr::Transform::CreateInstancedTransformData(&camWorldMatrix, nullptr));
				}

				// Coordinate axis at camera origin:
				if (!m_cameraAxisBatches.contains(camID))
				{
					m_cameraAxisBatches.emplace(
						camID, 
						BuildAxisBatch(
							re::Lifetime::Permanent, 
							m_cameraCoordinateAxisScale, 
							m_xAxisColor, 
							m_yAxisColor, 
							m_zAxisColor,
							m_axisOpacity));

					m_cameraAxisBatches.at(camID)->SetBuffer(m_cameraAxisTransformBuffers.at(camID));
				}
				m_debugLineStage->AddBatch(*m_cameraAxisBatches.at(camID));


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
							gr::Transform::CreateInstancedTransformBuffer(
								re::Lifetime::Permanent,
								re::Buffer::StagingPool::Permanent,
								&invViewProjMats.at(faceIdx),
								nullptr);
					}
					else if (camDataIsDirty)
					{
						m_cameraFrustumTransformBuffers.at(camID)[faceIdx].GetBuffer()->Commit(
							gr::Transform::CreateInstancedTransformData(&invViewProjMats.at(faceIdx), nullptr));
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

					m_debugLineStage->AddBatch(*m_cameraFrustumBatches.at(camID)[faceIdx]);
				}
			}
		}
		else
		{
			m_cameraAxisBatches.clear();
			m_cameraAxisTransformBuffers.clear();
			m_cameraFrustumBatches.clear();
			m_cameraFrustumTransformBuffers.clear();
		}

		if (m_showDeferredLightWireframe)
		{
			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			while (pointItr != pointItrEnd)
			{
				const gr::RenderDataID pointID = pointItr.GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(pointID))
				{
					gr::Transform::RenderData const& transformData = pointItr.GetTransformData();
					glm::mat4 const& lightTRS = transformData.g_model;

					if (!m_deferredLightWireframeTransformBuffers.contains(pointID))
					{
						m_deferredLightWireframeTransformBuffers.emplace(
							pointID,
							gr::Transform::CreateInstancedTransformBuffer(
								re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, &lightTRS, nullptr));
					}
					else
					{
						m_deferredLightWireframeTransformBuffers.at(pointID).GetBuffer()->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTRS, nullptr));
					}

					if (!m_deferredLightWireframeBatches.contains(pointID))
					{
						gr::MeshPrimitive::RenderData const& meshPrimData = pointItr.Get<gr::MeshPrimitive::RenderData>();

						m_deferredLightWireframeBatches.emplace(
							pointID,
							BuildWireframeBatch(
								re::Lifetime::Permanent, meshPrimData, m_deferredLightwireframeColor));

						m_deferredLightWireframeBatches.at(pointID)->SetBuffer(
							m_deferredLightWireframeTransformBuffers.at(pointID));
					}
					m_debugTriangleStage->AddBatch(*m_deferredLightWireframeBatches.at(pointID));
				}

				++pointItr;
			}

			auto spotItr = renderData.ObjectBegin<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>();
			auto const& spotItrEnd = renderData.ObjectEnd<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>();
			while (spotItr != spotItrEnd)
			{
				const gr::RenderDataID spotID = spotItr.GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(spotID))
				{
					gr::Transform::RenderData const& transformData = spotItr.GetTransformData();
					glm::mat4 const& lightTRS = transformData.g_model;

					if (!m_deferredLightWireframeTransformBuffers.contains(spotID))
					{
						m_deferredLightWireframeTransformBuffers.emplace(
							spotID,
							gr::Transform::CreateInstancedTransformBuffer(
								re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, &lightTRS, nullptr));
					}
					else
					{
						m_deferredLightWireframeTransformBuffers.at(spotID).GetBuffer()->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTRS, nullptr));
					}

					if (!m_deferredLightWireframeBatches.contains(spotID))
					{
						gr::MeshPrimitive::RenderData const& meshPrimData = spotItr.Get<gr::MeshPrimitive::RenderData>();

						m_deferredLightWireframeBatches.emplace(
							spotID,
							BuildWireframeBatch(
								re::Lifetime::Permanent, meshPrimData, m_deferredLightwireframeColor));

						m_deferredLightWireframeBatches.at(spotID)->SetBuffer(
							m_deferredLightWireframeTransformBuffers.at(spotID));
					}
					m_debugTriangleStage->AddBatch(*m_deferredLightWireframeBatches.at(spotID));
				}

				++spotItr;
			}
		}
		else
		{
			m_deferredLightWireframeBatches.clear();
			m_deferredLightWireframeTransformBuffers.clear();
		}

		if (m_showLightCoordinateAxis)
		{
			auto CreateUpdateLightCSAxisTransformBuffer = [&](
				gr::RenderDataID lightID, gr::Transform::RenderData const& transformData)
				{
					// Adjust the scale of the light's global TRS matrix basis vectors:
					glm::mat4 const& lightTR =
						AdjustMat4Scale(transformData.g_model, transformData.m_globalScale, m_lightCoordinateAxisScale);

					if (!m_lightCoordinateAxisTransformBuffers.contains(lightID))
					{
						m_lightCoordinateAxisTransformBuffers.emplace(
							lightID,
							gr::Transform::CreateInstancedTransformBuffer(
								re::Lifetime::Permanent, re::Buffer::StagingPool::Permanent, &lightTR, nullptr));
					}
					else
					{
						m_lightCoordinateAxisTransformBuffers.at(lightID).GetBuffer()->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTR, nullptr));
					}
				};

			auto BuildLightAxisBatch = [&](
				gr::RenderDataID lightID, gr::Transform::RenderData const& transformData)
				{
					if (!m_lightCoordinateAxisBatches.contains(lightID))
					{
						m_lightCoordinateAxisBatches.emplace(
							lightID,
							BuildAxisBatch(
								re::Lifetime::Permanent,
								m_lightCoordinateAxisScale,
								m_xAxisColor,
								m_yAxisColor,
								m_zAxisColor,
								m_axisOpacity));

						m_lightCoordinateAxisBatches.at(lightID)->SetBuffer(
							m_lightCoordinateAxisTransformBuffers.at(lightID));
					}
				};

			auto directionalItr = renderData.ObjectBegin<gr::Light::RenderDataDirectional>();
			auto const& directionalItrEnd = renderData.ObjectEnd<gr::Light::RenderDataDirectional>();
			while (directionalItr != directionalItrEnd)
			{
				const gr::RenderDataID lightID = directionalItr.GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					gr::Transform::RenderData const& transformData = directionalItr.GetTransformData();

					CreateUpdateLightCSAxisTransformBuffer(lightID, transformData);
					BuildLightAxisBatch(lightID, transformData);

					m_debugLineStage->AddBatch(*m_lightCoordinateAxisBatches.at(lightID));
				}

				++directionalItr;
			}

			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			while (pointItr != pointItrEnd)
			{
				const gr::RenderDataID lightID = pointItr.GetRenderDataID();
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					gr::Transform::RenderData const& transformData = pointItr.GetTransformData();

					CreateUpdateLightCSAxisTransformBuffer(lightID, transformData);
					BuildLightAxisBatch(lightID, transformData);

					m_debugLineStage->AddBatch(*m_lightCoordinateAxisBatches.at(lightID));
				}

				++pointItr;
			}

			auto spotItr = renderData.ObjectBegin<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>();
			auto const& spotItrEnd = renderData.ObjectEnd<gr::Light::RenderDataSpot, gr::MeshPrimitive::RenderData>();
			while (spotItr != spotItrEnd)
			{
				const gr::RenderDataID lightID = spotItr.GetRenderDataID();

				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(lightID))
				{
					gr::Transform::RenderData const& transformData = spotItr.GetTransformData();

					CreateUpdateLightCSAxisTransformBuffer(lightID, transformData);
					BuildLightAxisBatch(lightID, transformData);

					m_debugLineStage->AddBatch(*m_lightCoordinateAxisBatches.at(lightID));
				}

				++spotItr;
			}
		}
		else
		{
			m_lightCoordinateAxisBatches.clear();
			m_lightCoordinateAxisTransformBuffers.clear();
		}
	}


	DebugData DebugGraphicsSystem::PackDebugData() const
	{
		return DebugData{
				.g_axisScales = glm::vec4(
					m_worldCoordinateAxisScale,
					m_meshCoordinateAxisScale,
					m_lightCoordinateAxisScale,
					m_cameraCoordinateAxisScale),
				.g_scales = glm::vec4(m_vertexNormalsScale, 0.f, 0.f, 0.f),
				.g_colors = {
					glm::vec4(m_xAxisColor, m_axisOpacity),	// X: Red
					glm::vec4(m_yAxisColor, m_axisOpacity),	// Y: Green
					glm::vec4(m_zAxisColor, m_axisOpacity),	// Z: Blue
					m_normalsColor},
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

		m_isDirty |= ImGui::Checkbox(std::format("Show origin coordinate XYZ axis").c_str(), &m_showWorldCoordinateAxis);
		if (m_showWorldCoordinateAxis)
		{
			m_isDirty |= ImGui::SliderFloat("Coordinate axis scale", &m_worldCoordinateAxisScale, 0.f, 20.f);
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show mesh coordinate axis").c_str(), &m_showMeshCoordinateAxis);
		if (m_showMeshCoordinateAxis)
		{
			m_isDirty |= ImGui::SliderFloat("Mesh coordinate axis scale", &m_meshCoordinateAxisScale, 0.f, 20.f);
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show light coordinate axis").c_str(), &m_showLightCoordinateAxis);
		if (m_showLightCoordinateAxis)
		{
			m_isDirty |= ImGui::SliderFloat("Light coordinate axis scale", &m_lightCoordinateAxisScale, 0.f, 20.f);
		}

		if (m_showWorldCoordinateAxis || m_showMeshCoordinateAxis || m_showLightCoordinateAxis)
		{
			m_isDirty |= ImGui::SliderFloat("Axis opacity", &m_axisOpacity, 0.f, 1.f);
		}

		auto ShowColorPicker = [](bool doShow, glm::vec4& color) -> bool
			{
				bool isDirty = false;
				if (doShow)
				{
					ImGui::Indent();
					isDirty |= ImGui::ColorEdit4("Line color", &color.x, k_colorPickerFlags);
					ImGui::Unindent();
				}
				return isDirty;
			};

		m_isDirty |= ImGui::Checkbox(std::format("Show scene bounding box").c_str(), &m_showSceneBoundingBox);
		m_isDirty |= ShowColorPicker(m_showSceneBoundingBox, m_sceneBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show Mesh bounding boxes").c_str(), &m_showAllMeshBoundingBoxes);
		m_isDirty |= ShowColorPicker(m_showAllMeshBoundingBoxes, m_meshBoundsColor);

		m_isDirty |= ImGui::Checkbox(std::format("Show MeshPrimitive bounding boxes").c_str(), &m_showAllMeshPrimitiveBoundingBoxes);
		m_isDirty |= ShowColorPicker(m_showAllMeshPrimitiveBoundingBoxes, m_meshPrimitiveBoundsColor);

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
			
			auto camItr = renderData.ObjectBegin<gr::Camera::RenderData>();
			auto const& camEnd = renderData.ObjectEnd<gr::Camera::RenderData>();
			while (camItr != camEnd)
			{
				const gr::RenderDataID camID = camItr.GetRenderDataID();
				gr::Camera::RenderData const* camData = &camItr.Get<gr::Camera::RenderData>();
				gr::Transform::RenderData const* transformData = &camItr.GetTransformData();

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

				++camItr;
			}

			m_isDirty |= ImGui::SliderFloat("Camera coordinate axis scale", &m_cameraCoordinateAxisScale, 0.f, 20.f);
			ImGui::Unindent();
		}
		else
		{
			m_showCameraFrustums = false;
			m_camerasToDebug.clear();
		}

		m_isDirty |= ImGui::Checkbox(std::format("Show mesh wireframes").c_str(), &m_showAllWireframe);

		m_isDirty |= ImGui::Checkbox(std::format("Show deferred light mesh wireframes").c_str(), &m_showDeferredLightWireframe);
	}
}