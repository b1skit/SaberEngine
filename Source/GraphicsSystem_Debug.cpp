// � 2023 Adam Badke. All rights reserved.
#include "BoundsRenderData.h"
#include "ConfigKeys.h"
#include "GraphicsSystem_Debug.h"
#include "GraphicsSystemManager.h"
#include "ImGuiUtils.h"
#include "LightRenderData.h"
#include "SceneManager.h"
#include "Shader.h"


namespace
{
	re::Batch BuildAxisBatch(
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

		std::shared_ptr<re::VertexStream> axisPositionStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(axisPositions));

		std::shared_ptr<re::VertexStream> axisColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
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

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, axisBatchGraphicsParams);
	}


	re::Batch BuildBoundingBoxBatch(gr::Bounds::RenderData const& bounds, glm::vec3 const& boxColor)
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


		std::shared_ptr<re::VertexStream> boxPositionsStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(boxPositions));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(boxColors));

		std::shared_ptr<re::VertexStream> boxIndexStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
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

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, boundingBoxBatchGraphicsParams);
	}


	re::Batch BuildVertexNormalsBatch(
		gr::MeshPrimitive::RenderData const& meshPrimRenderData, 
		float scale,
		glm::vec3 const& globalScale,
		glm::vec3 const& normalColor)
	{
		re::VertexStream const* positionStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Position];
		re::VertexStream const* normalStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Normal];
		
		SEAssert(positionStream && normalStream, "Must have a position and normal stream");

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
		
		std::shared_ptr<re::VertexStream> normalPositionsStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(linePositions));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
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

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, boundingBoxBatchGraphicsParams);
	}


	
	re::Batch BuildCameraFrustumBatch(
		gr::Camera::RenderData const* camData,
		gr::Transform::RenderData const* transformData,
		glm::vec3 const& frustumColor, 
		glm::mat4 const& invViewProj)
	{
		// Convert NDC coordinates to world space
		glm::vec4 farTL = invViewProj * glm::vec4(-1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBL = invViewProj * glm::vec4(-1.f, -1.f, 1.f, 1.f);
		glm::vec4 farTR = invViewProj * glm::vec4(1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBR = invViewProj * glm::vec4(1.f, -1.f, 1.f, 1.f);
		glm::vec4 nearTL = invViewProj * glm::vec4(-1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBL = invViewProj * glm::vec4(-1.f, -1.f, 0.f, 1.f);
		glm::vec4 nearTR = invViewProj * glm::vec4(1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBR = invViewProj * glm::vec4(1.f, -1.f, 0.f, 1.f);

		farTL /= farTL.w;
		farBL /= farBL.w;
		farTR /= farTR.w;
		farBR /= farBR.w;
		nearTL /= nearTL.w;
		nearBL /= nearBL.w;
		nearTR /= nearTR.w;
		nearBR /= nearBR.w;

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

		std::shared_ptr<re::VertexStream> frustumPositionsStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(frustumPositions));

		std::shared_ptr<re::VertexStream> frustumColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(frustumColors));

		std::shared_ptr<re::VertexStream> frustumIndexStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
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

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, frustumBatchGraphicsParams);
	}
	


	re::Batch BuildWireframeBatch(gr::MeshPrimitive::RenderData const& meshPrimRenderData, glm::vec3 const& meshColor)
	{
		re::VertexStream const* positionStream = meshPrimRenderData.m_vertexStreams[gr::MeshPrimitive::Slot::Position];
		re::VertexStream const* indexStream = meshPrimRenderData.m_indexStream;
		SEAssert(positionStream && indexStream, "Must have a position and index stream");

		const glm::vec4 meshColorVec4 = glm::vec4(meshColor, 1.f);
		std::vector<glm::vec4> meshColors = std::vector<glm::vec4>(positionStream->GetNumElements(), meshColorVec4);

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(meshColors));

		re::Batch::GraphicsParams wireframeBatchGraphicsParams{};
		wireframeBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::IndexedInstanced;
		wireframeBatchGraphicsParams.m_numInstances = 1; // TODO: Support instancing
		wireframeBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::TriangleList;

		wireframeBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position] = positionStream;
		wireframeBatchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();
		wireframeBatchGraphicsParams.m_indexStream = indexStream;

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, wireframeBatchGraphicsParams);
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
		m_debugStage = re::RenderStage::CreateGraphicsStage("Debug stage", gfxStageParams);
	}


	void DebugGraphicsSystem::Create(re::StagePipeline& stagePipeline)
	{
		re::PipelineState debugPipelineState;
		debugPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		m_debugStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		re::PipelineState debugLinePipelineState;
		debugLinePipelineState.SetTopologyType(re::PipelineState::TopologyType::Line);
		debugLinePipelineState.SetFillMode(re::PipelineState::FillMode::Wireframe);
		debugLinePipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugLinePipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);
		m_debugStage->SetStageShader(re::Shader::GetOrCreate(en::ShaderNames::k_lineShaderName, debugLinePipelineState));

		m_debugStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		stagePipeline.AppendRenderStage(m_debugStage);
	}


	void DebugGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void DebugGraphicsSystem::CreateBatches()
	{
		constexpr glm::mat4 k_identity = glm::mat4(1.f);

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (m_showWorldCoordinateAxis)
		{
			re::Batch coordinateAxis = 
				BuildAxisBatch(m_worldCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor);

			std::shared_ptr<re::ParameterBlock> identityTransformPB =
				gr::Transform::CreateInstancedTransformParams(&k_identity, nullptr);
			coordinateAxis.SetParameterBlock(identityTransformPB);
			m_debugStage->AddBatch(coordinateAxis);
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
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(meshPrimItr.GetRenderDataID()))
				{
					gr::MeshPrimitive::RenderData const& meshPrimRenderData = meshPrimItr.Get<gr::MeshPrimitive::RenderData>();
					gr::Bounds::RenderData const& boundsRenderData = meshPrimItr.Get<gr::Bounds::RenderData>();

					gr::Transform::RenderData const& transformData = meshPrimItr.GetTransformDataFromTransformID();

					std::shared_ptr<re::ParameterBlock> meshTransformPB =
						gr::Transform::CreateInstancedTransformParams(transformData);

					// MeshPrimitives:
					if (m_showAllMeshPrimitiveBoundingBoxes || m_showAllVertexNormals || m_showAllWireframe)
					{
						if (m_showAllMeshPrimitiveBoundingBoxes && 
							gr::HasFeature(gr::RenderObjectFeature::IsMeshPrimitiveBounds, meshPrimItr.GetFeatureBits()))
						{
							re::Batch primitiveBoundsBatch =
								BuildBoundingBoxBatch(boundsRenderData, m_meshPrimitiveBoundsColor);
							primitiveBoundsBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(primitiveBoundsBatch);
						}

						if (m_showAllVertexNormals)
						{
							re::Batch vertexNormalsBatch = BuildVertexNormalsBatch(
								meshPrimRenderData, m_vertexNormalsScale, transformData.m_globalScale, m_normalsColor);
							vertexNormalsBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(vertexNormalsBatch);
						}

						if (m_showAllWireframe)
						{
							re::Batch wireframeBatch = BuildWireframeBatch(meshPrimRenderData, m_wireframeColor);
							wireframeBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(wireframeBatch);
						}
					}

					if (m_showMeshCoordinateAxis)
					{
						re::Batch meshCoordinateAxisBatch = BuildAxisBatch(
							m_meshCoordinateAxisScale, 
							m_xAxisColor, 
							m_yAxisColor, 
							m_zAxisColor, 
							transformData.m_globalScale);
						meshCoordinateAxisBatch.SetParameterBlock(meshTransformPB);
						m_debugStage->AddBatch(meshCoordinateAxisBatch);
					}
				}
				++meshPrimItr;
			}
		}

		// Mesh: Draw this after MeshPrimitive bounds so they're on top if the bounding box is the same
		if (m_showAllMeshBoundingBoxes)
		{
			auto boundsItr = renderData.ObjectBegin<gr::Bounds::RenderData>();
			auto boundsItrEnd = renderData.ObjectEnd<gr::Bounds::RenderData>();
			while (boundsItr != boundsItrEnd)
			{
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(boundsItr.GetRenderDataID()))
				{
					if (gr::HasFeature(gr::RenderObjectFeature::IsMeshBounds, boundsItr.GetFeatureBits()))
					{
						gr::Bounds::RenderData const& boundsRenderData = boundsItr.Get<gr::Bounds::RenderData>();

						std::shared_ptr<re::ParameterBlock> boundsTransformPB =
							gr::Transform::CreateInstancedTransformParams(boundsItr.GetTransformDataFromTransformID());

						re::Batch boundingBoxBatch = BuildBoundingBoxBatch(boundsRenderData, m_meshBoundsColor);
						boundingBoxBatch.SetParameterBlock(boundsTransformPB);
						m_debugStage->AddBatch(boundingBoxBatch);
					}
				}
				++boundsItr;
			}
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

					std::shared_ptr<re::ParameterBlock> boundsTransformPB =
						gr::Transform::CreateInstancedTransformParams(boundsItr.GetTransformDataFromTransformID());

					re::Batch boundingBoxBatch = BuildBoundingBoxBatch(boundsRenderData, m_sceneBoundsColor);
					boundingBoxBatch.SetParameterBlock(boundsTransformPB);
					m_debugStage->AddBatch(boundingBoxBatch);
				}
				++boundsItr;
			}
		}

		if (m_showCameraFrustums)
		{
			for (auto const& camData : m_camerasToDebug)
			{				
				// Use the inverse view matrix, as it omits any scale that might be present in the Transform hierarchy
				glm::mat4 const& camWorldMatrix = camData.first->m_cameraParams.g_invView;
				std::shared_ptr<re::ParameterBlock> cameraTransformPB =
					gr::Transform::CreateInstancedTransformParams(&camWorldMatrix, nullptr);

				// Coordinate axis at camera origin:
				re::Batch cameraCoordinateAxisBatch =
					BuildAxisBatch(m_cameraCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor);
				cameraCoordinateAxisBatch.SetParameterBlock(cameraTransformPB);
				m_debugStage->AddBatch(cameraCoordinateAxisBatch);

				// Our frustum points are already in world-space
				const glm::mat4 identityMat = glm::mat4(1.f);
				std::shared_ptr<re::ParameterBlock> identityPB =
					gr::Transform::CreateInstancedTransformParams(&identityMat, nullptr);

				const uint8_t numFrustums = camData.first->m_cameraConfig.m_projectionType ==
					gr::Camera::Config::ProjectionType::PerspectiveCubemap ? 6 : 1;

				std::vector<glm::mat4> invViewProjMats;
				invViewProjMats.reserve(numFrustums);

				if (numFrustums == 6)
				{
					std::vector<glm::mat4> const& viewMats = gr::Camera::BuildCubeViewMatrices(
						camData.second->m_globalPosition,
						camData.second->m_globalRight,
						camData.second->m_globalUp,
						camData.second->m_globalForward);

					std::vector<glm::mat4> const& viewProjMats =
						gr::Camera::BuildCubeViewProjectionMatrices(viewMats, camData.first->m_cameraParams.g_projection);

					invViewProjMats =
						gr::Camera::BuildCubeInvViewProjectionMatrices(viewProjMats);
				}
				else
				{
					invViewProjMats.emplace_back(camData.first->m_cameraParams.g_invViewProjection);
				}
				
				for (uint8_t faceIdx = 0; faceIdx < numFrustums; faceIdx++)
				{
					re::Batch camFrustumBatch = BuildCameraFrustumBatch(
						camData.first, camData.second, m_cameraFrustumColor, invViewProjMats[faceIdx]);

					camFrustumBatch.SetParameterBlock(identityPB);
					m_debugStage->AddBatch(camFrustumBatch);
				}
			}		
		}

		if (m_showDeferredLightWireframe)
		{
			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			while (pointItr != pointItrEnd)
			{
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(pointItr.GetRenderDataID()))
				{
					gr::Light::RenderDataPoint const& pointData =
						pointItr.Get<gr::Light::RenderDataPoint>();
					gr::Transform::RenderData const& transformData = pointItr.GetTransformDataFromTransformID();

					glm::mat4 const& lightTRS = transformData.g_model;

					std::shared_ptr<re::ParameterBlock> pointLightMeshTransformPB =
						gr::Transform::CreateInstancedTransformParams(&lightTRS, nullptr);

					gr::MeshPrimitive::RenderData const& meshPrimData = pointItr.Get<gr::MeshPrimitive::RenderData>();

					re::Batch pointLightWireframeBatch = BuildWireframeBatch(
						meshPrimData,
						m_deferredLightwireframeColor);
					pointLightWireframeBatch.SetParameterBlock(pointLightMeshTransformPB);
					m_debugStage->AddBatch(pointLightWireframeBatch);
				}

				++pointItr;
			}
		}

		if (m_showLightCoordinateAxis)
		{
			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint, gr::MeshPrimitive::RenderData>();
			while (pointItr != pointItrEnd)
			{
				if (m_selectedRenderDataIDs.empty() || m_selectedRenderDataIDs.contains(pointItr.GetRenderDataID()))
				{
					gr::Transform::RenderData const& transformData = pointItr.GetTransformDataFromTransformID();
					glm::mat4 const& lightTRS = transformData.g_model;

					std::shared_ptr<re::ParameterBlock> pointLightMeshTransformPB =
						gr::Transform::CreateInstancedTransformParams(&lightTRS, nullptr);

					re::Batch coordinateAxis = BuildAxisBatch(
						m_lightCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor, transformData.m_globalScale);

					coordinateAxis.SetParameterBlock(pointLightMeshTransformPB);
					m_debugStage->AddBatch(coordinateAxis);
				}

				++pointItr;
			}
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
				gr::Camera::RenderData const* camData = &camItr.Get<gr::Camera::RenderData>();
				gr::Transform::RenderData const* transformData = &camItr.GetTransformDataFromTransformID();

				const bool cameraAlreadyAdded = m_camerasToDebug.contains(camData);
				bool cameraSelected = cameraAlreadyAdded;
				if (ImGui::Checkbox(
						std::format("{}##", camData->m_cameraName, util::PtrToID(camData)).c_str(), &cameraSelected) &&
					!cameraAlreadyAdded)
				{
					m_camerasToDebug.emplace(std::make_pair(camData, transformData));
				}
				else if (cameraAlreadyAdded && !cameraSelected)
				{
					m_camerasToDebug.erase(camData);
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