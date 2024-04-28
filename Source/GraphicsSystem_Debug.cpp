// © 2023 Adam Badke. All rights reserved.
#include "BoundsRenderData.h"
#include "Core\ConfigKeys.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystemManager.h"
#include "Core\Util\ImGuiUtils.h"
#include "LightRenderData.h"
#include "Shader.h"


namespace
{
	re::VertexStream::Lifetime GetVertexStreamLifetimeFromBatchLifetime(re::Batch::Lifetime batchLifetime)
	{
		switch (batchLifetime)
		{
		case re::Batch::Lifetime::SingleFrame: return re::VertexStream::Lifetime::SingleFrame;
		case re::Batch::Lifetime::Permanent: return re::VertexStream::Lifetime::Permanent;
		default: SEAssertF("Invalid batch lifetime");
		}
		return re::VertexStream::Lifetime::SingleFrame;
	}


	std::unique_ptr<re::Batch> BuildAxisBatch(
		re::Batch::Lifetime batchLifetime,
		float axisScale, 
		glm::vec3 const& xAxisColor, 
		glm::vec3 const& yAxisColor, 
		glm::vec3 const& zAxisColor, 
		glm::vec3 transformGlobalScale = glm::vec3(1.f)) // Used to prevent scale affecting axis size
	{
		std::vector<glm::vec3> axisPositions = { 
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisX * axisScale / transformGlobalScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisY * axisScale / transformGlobalScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisZ * axisScale / transformGlobalScale,
		};

		std::vector<glm::vec4> axisColors = { 
			glm::vec4(xAxisColor, 1.f), glm::vec4(xAxisColor, 1.f),
			glm::vec4(yAxisColor, 1.f), glm::vec4(yAxisColor, 1.f),
			glm::vec4(zAxisColor, 1.f), glm::vec4(zAxisColor, 1.f),
		};

		const re::VertexStream::Lifetime streamLifetime = GetVertexStreamLifetimeFromBatchLifetime(batchLifetime);

		std::shared_ptr<re::VertexStream> axisPositionStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(axisPositions));

		std::shared_ptr<re::VertexStream> axisColorStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(axisColors));

		re::Batch::GraphicsParams axisBatchGraphicsParams{};
		axisBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		axisBatchGraphicsParams.m_numInstances = 1;
		axisBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;
		
		axisBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = axisPositionStream.get();
		axisBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = axisColorStream.get();

		return std::make_unique<re::Batch>(batchLifetime, axisBatchGraphicsParams);
	}


	std::unique_ptr<re::Batch> BuildBoundingBoxBatch(
		re::Batch::Lifetime batchLifetime, gr::Bounds::RenderData const& bounds, glm::vec3 const& boxColor)
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

		//									   0, 1, 2, 3, 4, 5, 6, 7
		std::vector<glm::vec3> boxPositions = {a, b, c, d, e, f, g, h};

		const glm::vec4 boxColorVec4 = glm::vec4(boxColor, 1.f);
		std::vector<glm::vec4> boxColors = std::vector<glm::vec4>(boxPositions.size(), boxColorVec4);

		std::vector<uint32_t> boxIndexes = {
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
		};

		const re::VertexStream::Lifetime streamLifetime = GetVertexStreamLifetimeFromBatchLifetime(batchLifetime);

		std::shared_ptr<re::VertexStream> boxPositionsStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(boxPositions));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(boxColors));

		std::shared_ptr<re::VertexStream> boxIndexStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Index,
			1, // numComponents per element
			re::VertexStream::DataType::UInt,
			re::VertexStream::Normalize::False,
			std::move(boxIndexes));

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		boundingBoxBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = boxPositionsStream.get();
		boundingBoxBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();
		boundingBoxBatchGraphicsParams.m_indexStream = boxIndexStream.get();

		return std::make_unique<re::Batch>(batchLifetime, boundingBoxBatchGraphicsParams);
	}


	std::unique_ptr<re::Batch> BuildVertexNormalsBatch(
		re::Batch::Lifetime batchLifetime,
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		float scale,
		glm::vec3 const& globalScale,
		glm::vec3 const& normalColor)
	{
		if (meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Normal] == nullptr)
		{
			return nullptr; // No normals? Nothing to build
		}

		re::VertexStream const* positionStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Position];
		re::VertexStream const* normalStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Normal];

		std::vector<glm::vec3> linePositions;

		SEAssert(positionStream->GetDataType() == re::VertexStream::DataType::Float && 
			positionStream->GetNumComponents() == 3 &&
			normalStream->GetDataType() == re::VertexStream::DataType::Float &&
			normalStream->GetNumComponents() == 3,
			"Unexpected position or normal data");
		
		// Build lines between the position and position + normal offset:
		glm::vec3 const* positionData = static_cast<glm::vec3 const*>(positionStream->GetData());
		glm::vec3 const* normalData = static_cast<glm::vec3 const*>(normalStream->GetData());
		for (uint32_t elementIdx = 0; elementIdx < positionStream->GetNumElements(); elementIdx++)
		{
			linePositions.emplace_back(positionData[elementIdx]);
			linePositions.emplace_back(positionData[elementIdx] + normalData[elementIdx] * scale / globalScale);
		}
		
		const glm::vec4 normalColorVec4 = glm::vec4(normalColor, 1.f);
		std::vector<glm::vec4> normalColors = std::vector<glm::vec4>(linePositions.size(), normalColorVec4);
		
		const re::VertexStream::Lifetime streamLifetime = GetVertexStreamLifetimeFromBatchLifetime(batchLifetime);

		std::shared_ptr<re::VertexStream> normalPositionsStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(linePositions));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(normalColors));

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		boundingBoxBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = normalPositionsStream.get();
		boundingBoxBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();

		return std::make_unique<re::Batch>(batchLifetime, boundingBoxBatchGraphicsParams);
	}

	
	std::unique_ptr<re::Batch> BuildCameraFrustumBatch(
		re::Batch::Lifetime batchLifetime,
		gr::Transform::RenderData const* transformData,
		glm::vec3 const& frustumColor)
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

		//											  0		1		2	   3	  4		  5		  6		  7
		std::vector<glm::vec3> frustumPositions = { farTL, farBL, farTR, farBR, nearTL, nearBL, nearTR, nearBR };

		const glm::vec4 fustumColorVec4 = glm::vec4(frustumColor, 1.f);
		std::vector<glm::vec4> frustumColors = { frustumPositions.size(), fustumColorVec4 };

		std::vector<uint32_t> frustumIndexes = {
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
		};

		const re::VertexStream::Lifetime streamLifetime = GetVertexStreamLifetimeFromBatchLifetime(batchLifetime);

		std::shared_ptr<re::VertexStream> frustumPositionsStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(frustumPositions));

		std::shared_ptr<re::VertexStream> frustumColorStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(frustumColors));

		std::shared_ptr<re::VertexStream> frustumIndexStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Index,
			1, // numComponents per element
			re::VertexStream::DataType::UInt,
			re::VertexStream::Normalize::False,
			std::move(frustumIndexes));

		re::Batch::GraphicsParams frustumBatchGraphicsParams{};
		frustumBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		frustumBatchGraphicsParams.m_numInstances = 1;
		frustumBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		frustumBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = frustumPositionsStream.get();
		frustumBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = frustumColorStream.get();
		frustumBatchGraphicsParams.m_indexStream = frustumIndexStream.get();

		return std::make_unique<re::Batch>(batchLifetime, frustumBatchGraphicsParams);
	}
	

	std::unique_ptr<re::Batch> BuildWireframeBatch(
		re::Batch::Lifetime batchLifetime, 
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		glm::vec3 const& meshColor)
	{
		re::VertexStream const* positionStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Position];
		re::VertexStream const* indexStream = meshPrimRenderData.m_indexStream;
		SEAssert(positionStream && indexStream, "Must have a position and index stream");

		const glm::vec4 meshColorVec4 = glm::vec4(meshColor, 1.f);
		std::vector<glm::vec4> meshColors = std::vector<glm::vec4>(positionStream->GetNumElements(), meshColorVec4);

		const re::VertexStream::Lifetime streamLifetime = GetVertexStreamLifetimeFromBatchLifetime(batchLifetime);

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			streamLifetime,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(meshColors));

		re::Batch::GraphicsParams wireframeBatchGraphicsParams{};
		wireframeBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		wireframeBatchGraphicsParams.m_numInstances = 1;
		wireframeBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::TriangleList;

		wireframeBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = positionStream;
		wireframeBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();
		wireframeBatchGraphicsParams.m_indexStream = indexStream;

		return std::make_unique<re::Batch>(batchLifetime, wireframeBatchGraphicsParams);
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Debug Graphics System";

	DebugGraphicsSystem::DebugGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_debugLineStage = re::RenderStage::CreateGraphicsStage("Debug line stage", gfxStageParams);
		m_debugTriangleStage = re::RenderStage::CreateGraphicsStage("Debug triangle stage", gfxStageParams);
	}


	void DebugGraphicsSystem::InitPipeline(re::StagePipeline& stagePipeline, TextureDependencies const& texDependencies)
	{
		re::PipelineState debugPipelineState;
		debugPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		// Line topology stage:
		m_debugLineStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer
		
		re::PipelineState debugLinePipelineState;
		debugLinePipelineState.SetTopologyType(re::PipelineState::TopologyType::Line);
		debugLinePipelineState.SetFillMode(re::PipelineState::FillMode::Wireframe);
		debugLinePipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugLinePipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);
		m_debugLineStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_lineShaderName, debugLinePipelineState));

		m_debugLineStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		stagePipeline.AppendRenderStage(m_debugLineStage);
		
		// Triangle topology stage:
		m_debugTriangleStage->SetTextureTargetSet(nullptr);

		re::PipelineState debugTrianglePipelineState = debugLinePipelineState;
		debugTrianglePipelineState.SetTopologyType(re::PipelineState::TopologyType::Triangle);

		m_debugTriangleStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_lineShaderName, debugTrianglePipelineState));

		m_debugTriangleStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		stagePipeline.AppendRenderStage(m_debugTriangleStage);
	}


	void DebugGraphicsSystem::PreRender(DataDependencies const&)
	{
		CreateBatches();
	}


	void DebugGraphicsSystem::CreateBatches()
	{
		constexpr glm::mat4 k_identity = glm::mat4(1.f);

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (m_showWorldCoordinateAxis)
		{
			if (m_worldCoordinateAxisBatch == nullptr)
			{
				m_worldCoordinateAxisBatch = std::move(BuildAxisBatch(re::Batch::Lifetime::Permanent, 
						m_worldCoordinateAxisScale, 
						m_xAxisColor, 
						m_yAxisColor, 
						m_zAxisColor));

				std::shared_ptr<re::Buffer> identityTransformBuffer = gr::Transform::CreateInstancedTransformBuffer(
					re::Buffer::Type::Immutable, &k_identity, nullptr);

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

					// Create/update a cached buffer:
					if (!m_meshPrimTransformBuffers.contains(meshPrimRenderDataID))
					{
						m_meshPrimTransformBuffers.emplace(
							meshPrimRenderDataID, 
							gr::Transform::CreateInstancedTransformBuffer(
								re::Buffer::Type::Mutable, 
								transformData));
					}
					else
					{
						m_meshPrimTransformBuffers.at(meshPrimRenderDataID)->Commit(
							gr::Transform::CreateInstancedTransformData(transformData));
					}
					std::shared_ptr<re::Buffer> meshTransformBuffer = 
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
										re::Batch::Lifetime::Permanent, boundsRenderData, m_meshPrimitiveBoundsColor));

								m_meshPrimBoundingBoxBatches.at(meshPrimRenderDataID)->SetBuffer(meshTransformBuffer);
							}
							m_debugLineStage->AddBatch(*m_meshPrimBoundingBoxBatches.at(meshPrimRenderDataID));
						}

						if (m_showAllVertexNormals)
						{
							if (!m_vertexNormalBatches.contains(meshPrimRenderDataID))
							{
								std::unique_ptr<re::Batch> normalsBatch = BuildVertexNormalsBatch(
									re::Batch::Lifetime::Permanent,
									meshPrimRenderData,
									m_vertexNormalsScale,
									transformData.m_globalScale,
									m_normalsColor);

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
										re::Batch::Lifetime::Permanent, meshPrimRenderData, m_wireframeColor));

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
									re::Batch::Lifetime::Permanent,
									m_meshCoordinateAxisScale,
									m_xAxisColor,
									m_yAxisColor,
									m_zAxisColor,
									transformData.m_globalScale)));

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
									re::Buffer::Type::Mutable, boundsItr.GetTransformData()));
						}
						else
						{
							m_meshBoundingBoxBuffers.at(meshID)->Commit(
								gr::Transform::CreateInstancedTransformData(boundsItr.GetTransformData()));
						}

						if (!m_meshBoundingBoxBatches.contains(meshID))
						{
							m_meshBoundingBoxBatches.emplace(meshID,
								BuildBoundingBoxBatch(re::Batch::Lifetime::Permanent,boundsRenderData, m_meshBoundsColor));

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

					if (m_sceneBoundsTransformBuffer == nullptr)
					{
						m_sceneBoundsTransformBuffer = gr::Transform::CreateInstancedTransformBuffer(
							re::Buffer::Type::Mutable, boundsItr.GetTransformData());
					}

					if (m_sceneBoundsBatch == nullptr)
					{
						m_sceneBoundsBatch =
							BuildBoundingBoxBatch(re::Batch::Lifetime::Permanent, boundsRenderData, m_sceneBoundsColor);

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
			m_sceneBoundsTransformBuffer = nullptr;
		}

		if (m_showCameraFrustums)
		{
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
						re::Buffer::Type::Mutable, &camWorldMatrix, nullptr));
				}
				else if (camDataIsDirty)
				{
					m_cameraAxisTransformBuffers.at(camID)->Commit(
						gr::Transform::CreateInstancedTransformData(&camWorldMatrix, nullptr));
				}

				// Coordinate axis at camera origin:
				if (!m_cameraAxisBatches.contains(camID))
				{
					m_cameraAxisBatches.emplace(
						camID, 
						BuildAxisBatch(
							re::Batch::Lifetime::Permanent, 
							m_cameraCoordinateAxisScale, 
							m_xAxisColor, 
							m_yAxisColor, 
							m_zAxisColor));

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
					if (m_cameraFrustumTransformBuffers.at(camID)[faceIdx] == nullptr)
					{
						m_cameraFrustumTransformBuffers.at(camID)[faceIdx] =
							gr::Transform::CreateInstancedTransformBuffer(
								re::Buffer::Type::Mutable,
								&invViewProjMats.at(faceIdx),
								nullptr);
					}
					else if (camDataIsDirty)
					{
						m_cameraFrustumTransformBuffers.at(camID)[faceIdx]->Commit(
							gr::Transform::CreateInstancedTransformData(&invViewProjMats.at(faceIdx), nullptr));
					}

					if (m_cameraFrustumBatches.at(camID)[faceIdx] == nullptr)
					{
						m_cameraFrustumBatches.at(camID)[faceIdx] = std::move(BuildCameraFrustumBatch(
							re::Batch::Lifetime::Permanent,
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
								re::Buffer::Type::Mutable, &lightTRS, nullptr));
					}
					else
					{
						m_deferredLightWireframeTransformBuffers.at(pointID)->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTRS, nullptr));
					}

					if (!m_deferredLightWireframeBatches.contains(pointID))
					{
						gr::MeshPrimitive::RenderData const& meshPrimData = pointItr.Get<gr::MeshPrimitive::RenderData>();

						m_deferredLightWireframeBatches.emplace(
							pointID,
							BuildWireframeBatch(
								re::Batch::Lifetime::Permanent, meshPrimData, m_deferredLightwireframeColor));

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
								re::Buffer::Type::Mutable, &lightTRS, nullptr));
					}
					else
					{
						m_deferredLightWireframeTransformBuffers.at(spotID)->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTRS, nullptr));
					}

					if (!m_deferredLightWireframeBatches.contains(spotID))
					{
						gr::MeshPrimitive::RenderData const& meshPrimData = spotItr.Get<gr::MeshPrimitive::RenderData>();

						m_deferredLightWireframeBatches.emplace(
							spotID,
							BuildWireframeBatch(
								re::Batch::Lifetime::Permanent, meshPrimData, m_deferredLightwireframeColor));

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
					glm::mat4 const& lightTRS = transformData.g_model;

					if (!m_lightCoordinateAxisTransformBuffers.contains(lightID))
					{
						m_lightCoordinateAxisTransformBuffers.emplace(
							lightID,
							gr::Transform::CreateInstancedTransformBuffer(
								re::Buffer::Type::Mutable, &lightTRS, nullptr));
					}
					else
					{
						m_lightCoordinateAxisTransformBuffers.at(lightID)->Commit(
							gr::Transform::CreateInstancedTransformData(&lightTRS, nullptr));
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
								re::Batch::Lifetime::Permanent,
								m_lightCoordinateAxisScale,
								m_xAxisColor,
								m_yAxisColor,
								m_zAxisColor,
								transformData.m_globalScale));

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


	void DebugGraphicsSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader("Target render data objects"))
		{
			ImGui::Indent();

			static bool s_targetAll = true;
			if (ImGui::Button(std::format("{}", s_targetAll ? "Select specific IDs" : "Select all").c_str()))
			{
				s_targetAll = !s_targetAll;
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
					ImGui::Checkbox(std::format("{}", renderDataID).c_str(), &isSelected);

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

		ImGui::Checkbox(std::format("Show origin coordinate XYZ axis").c_str(), &m_showWorldCoordinateAxis);
		if (m_showWorldCoordinateAxis)
		{
			ImGui::SliderFloat("Coordinate axis scale", &m_worldCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show mesh coordinate axis").c_str(), &m_showMeshCoordinateAxis);
		if (m_showMeshCoordinateAxis)
		{
			ImGui::SliderFloat("Mesh coordinate axis scale", &m_meshCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show light coordinate axis").c_str(), &m_showLightCoordinateAxis);
		if (m_showLightCoordinateAxis)
		{
			ImGui::SliderFloat("Mesh coordinate axis scale", &m_lightCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show scene bounding box").c_str(), &m_showSceneBoundingBox);
		ImGui::Checkbox(std::format("Show Mesh bounding boxes").c_str(), &m_showAllMeshBoundingBoxes);
		ImGui::Checkbox(std::format("Show MeshPrimitive bounding boxes").c_str(), &m_showAllMeshPrimitiveBoundingBoxes);

		ImGui::Checkbox(std::format("Show vertex normals").c_str(), &m_showAllVertexNormals);
		if (m_showAllVertexNormals)
		{
			ImGui::SliderFloat("Vertex normals scale", &m_vertexNormalsScale, 0.f, 2.f);
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
				}
				else if (cameraAlreadyAdded && !cameraSelected)
				{
					m_camerasToDebug.erase(camID);
				}

				++camItr;
			}

			ImGui::SliderFloat("Camera coordinate axis scale", &m_cameraCoordinateAxisScale, 0.f, 20.f);
			ImGui::Unindent();
		}
		else
		{
			m_showCameraFrustums = false;
			m_camerasToDebug.clear();
		}

		ImGui::Checkbox(std::format("Show mesh wireframes").c_str(), &m_showAllWireframe);

		ImGui::Checkbox(std::format("Show deferred light mesh wireframes").c_str(), &m_showDeferredLightWireframe);
	}
}