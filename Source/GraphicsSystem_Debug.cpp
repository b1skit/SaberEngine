// © 2023 Adam Badke. All rights reserved.
#include "ConfigKeys.h"
#include "GraphicsSystem_Debug.h"
#include "SceneManager.h"


namespace
{
	re::Batch BuildAxisBatch(
		float axisScale, glm::vec3 const& xAxisColor, glm::vec3 const& yAxisColor, glm::vec3 const& zAxisColor)
	{
		std::vector<glm::vec3> axisPositions = { 
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisX * axisScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisY * axisScale,
			glm::vec3(0.f, 0.f, 0.f), gr::Transform::WorldAxisZ * axisScale,
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


	re::Batch BuildBoundingBoxBatch(gr::Bounds const& bounds, glm::vec3 const& boxColor)
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
		const glm::vec3 a = glm::vec3(bounds.xMin(), bounds.yMax(), bounds.zMax());
		const glm::vec3 b = glm::vec3(bounds.xMax(), bounds.yMax(), bounds.zMax());
		const glm::vec3 c = glm::vec3(bounds.xMin(), bounds.yMin(), bounds.zMax());
		const glm::vec3 d = glm::vec3(bounds.xMax(), bounds.yMin(), bounds.zMax());

		const glm::vec3 e = glm::vec3(bounds.xMin(), bounds.yMax(), bounds.zMin());
		const glm::vec3 f = glm::vec3(bounds.xMax(), bounds.yMax(), bounds.zMin());
		const glm::vec3 g = glm::vec3(bounds.xMin(), bounds.yMin(), bounds.zMin());
		const glm::vec3 h = glm::vec3(bounds.xMax(), bounds.yMin(), bounds.zMin());

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


	re::Batch BuildVertexNormalsBatch(gr::MeshPrimitive const* meshPrimitive, float scale, glm::vec3 const& normalColor)
	{
		re::VertexStream const* positionStream = meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position);
		re::VertexStream const* normalStream = meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Normal);
		
		SEAssert("Must have a position and normal stream", positionStream && normalStream);

		std::vector<glm::vec3> linePositions;

		SEAssert("Unexpected position or normal data", 
			positionStream->GetDataType() == re::VertexStream::DataType::Float && 
			positionStream->GetNumComponents() == 3 &&
			normalStream->GetDataType() == re::VertexStream::DataType::Float &&
			normalStream->GetNumComponents() == 3);

		// Build lines between the position and position + normal offset:
		glm::vec3 const* positionData = static_cast<glm::vec3 const*>(positionStream->GetData());
		glm::vec3 const* normalData = static_cast<glm::vec3 const*>(normalStream->GetData());
		for (uint32_t elementIdx = 0; elementIdx < positionStream->GetNumElements(); elementIdx++)
		{
			linePositions.emplace_back(positionData[elementIdx]);
			linePositions.emplace_back(positionData[elementIdx] + normalData[elementIdx] * scale);
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


	re::Batch BuildCameraFrustumBatch(gr::Camera* camera, glm::vec3 const& frustumColor)
	{
		// Convert NDC coordinates to world-space points:
		glm::vec4 farTL = camera->GetInverseViewProjectionMatrix() * glm::vec4(-1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBL = camera->GetInverseViewProjectionMatrix() * glm::vec4(-1.f, -1.f, 1.f, 1.f);
		glm::vec4 farTR = camera->GetInverseViewProjectionMatrix() * glm::vec4(1.f, 1.f, 1.f, 1.f);
		glm::vec4 farBR = camera->GetInverseViewProjectionMatrix() * glm::vec4(1.f, -1.f, 1.f, 1.f);
		glm::vec4 nearTL = camera->GetInverseViewProjectionMatrix() * glm::vec4(-1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBL = camera->GetInverseViewProjectionMatrix() * glm::vec4(-1.f, -1.f, 0.f, 1.f);
		glm::vec4 nearTR = camera->GetInverseViewProjectionMatrix() * glm::vec4(1.f, 1.f, 0.f, 1.f);
		glm::vec4 nearBR = camera->GetInverseViewProjectionMatrix() * glm::vec4(1.f, -1.f, 0.f, 1.f);

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


	re::Batch BuildWireframeBatch(gr::MeshPrimitive const* meshPrimitive, glm::vec3 const& meshColor)
	{
		re::VertexStream const* positionStream = meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position);
		re::VertexStream const* indexStream = meshPrimitive->GetIndexStream();
		SEAssert("Must have a position and index stream", positionStream && indexStream);

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

	DebugGraphicsSystem::DebugGraphicsSystem()
		: GraphicsSystem(k_gsName)
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

		m_debugStage->AddPermanentParameterBlock(en::SceneManager::Get()->GetMainCamera()->GetCameraParams());

		re::PipelineState debugLinePipelineState;
		debugLinePipelineState.SetTopologyType(re::PipelineState::TopologyType::Line);
		debugLinePipelineState.SetFillMode(re::PipelineState::FillMode::Wireframe);
		debugLinePipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		debugLinePipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);
		m_debugStage->SetStageShader(re::Shader::GetOrCreate(en::ShaderNames::k_lineShaderName, debugLinePipelineState));

		m_debugStage->AddPermanentParameterBlock(en::SceneManager::Get()->GetMainCamera()->GetCameraParams());

		stagePipeline.AppendRenderStage(m_debugStage);
	}


	void DebugGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void DebugGraphicsSystem::CreateBatches()
	{
		constexpr glm::mat4 k_identity = glm::mat4(1.f);

		if (m_showWorldCoordinateAxis)
		{
			re::Batch coordinateAxis = 
				BuildAxisBatch(m_worldCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor);

			std::shared_ptr<re::ParameterBlock> identityTransformPB =
				gr::Mesh::CreateInstancedMeshParamsData(&k_identity, nullptr);
			coordinateAxis.SetParameterBlock(identityTransformPB);
			m_debugStage->AddBatch(coordinateAxis);
		}

		if (m_showAllMeshBoundingBoxes || 
			m_showAllMeshPrimitiveBoundingBoxes || 
			m_showMeshCoordinateAxis || 
			m_showAllVertexNormals ||
			m_showAllWireframe)
		{
			for (auto const& mesh : en::SceneManager::GetSceneData()->GetMeshes())
			{
				std::shared_ptr<re::ParameterBlock> meshTransformPB =
					gr::Mesh::CreateInstancedMeshParamsData(&mesh->GetTransform()->GetGlobalMatrix(gr::Transform::TRS), nullptr);
				
				// MeshPrimitives:
				if (m_showAllMeshPrimitiveBoundingBoxes || m_showAllVertexNormals || m_showAllWireframe)
				{
					for (auto const& meshPrimitive : mesh->GetMeshPrimitives())
					{
						if (m_showAllMeshPrimitiveBoundingBoxes)
						{
							re::Batch primitiveBoundsBatch =
								BuildBoundingBoxBatch(meshPrimitive->GetBounds(), m_meshPrimitiveBoundsColor);
							primitiveBoundsBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(primitiveBoundsBatch);
						}

						if (m_showAllVertexNormals)
						{
							re::Batch vertexNormalsBatch = 
								BuildVertexNormalsBatch(meshPrimitive.get(), m_vertexNormalsScale, m_normalsColor);
							vertexNormalsBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(vertexNormalsBatch);
						}

						if (m_showAllWireframe)
						{
							re::Batch wireframeBatch = BuildWireframeBatch(meshPrimitive.get(), m_wireframeColor);
							wireframeBatch.SetParameterBlock(meshTransformPB);
							m_debugStage->AddBatch(wireframeBatch);
						}
					}
				}

				// Mesh: Draw this 2nd so it's on top if the bounding box is the same as the MeshPrimitive
				if (m_showAllMeshBoundingBoxes)
				{
					re::Batch boundingBoxBatch = BuildBoundingBoxBatch(mesh->GetBounds(), m_meshBoundsColor);
					boundingBoxBatch.SetParameterBlock(meshTransformPB);
					m_debugStage->AddBatch(boundingBoxBatch);
				}

				if (m_showMeshCoordinateAxis)
				{
					re::Batch meshCoordinateAxisBatch = 
						BuildAxisBatch(m_meshCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor);
					meshCoordinateAxisBatch.SetParameterBlock(meshTransformPB);
					m_debugStage->AddBatch(meshCoordinateAxisBatch);
				}
			}
		}

		if (m_showAllCameraFrustums)
		{
			for (auto const& camera : en::SceneManager::GetSceneData()->GetCameras())
			{
				re::Batch camFrustumBatch = BuildCameraFrustumBatch(camera.get(), m_cameraFrustumColor);

				std::shared_ptr<re::ParameterBlock> cameraTransformPB =
					gr::Mesh::CreateInstancedMeshParamsData(&camera->GetTransform()->GetGlobalMatrix(gr::Transform::TRS), nullptr);
				camFrustumBatch.SetParameterBlock(cameraTransformPB);
				m_debugStage->AddBatch(camFrustumBatch);

				// Coordinate axis at camera origin:
				re::Batch cameraCoordinateAxisBatch =
					BuildAxisBatch(m_cameraCoordinateAxisScale, m_xAxisColor, m_yAxisColor, m_zAxisColor);
				cameraCoordinateAxisBatch.SetParameterBlock(cameraTransformPB);
				m_debugStage->AddBatch(cameraCoordinateAxisBatch);
			}			
		}
	}


	void DebugGraphicsSystem::ShowImGuiWindow()
	{
		ImGui::Checkbox(std::format("Show origin coordinate XYZ axis").c_str(), &m_showWorldCoordinateAxis);
		if (m_showWorldCoordinateAxis)
		{
			ImGui::SliderFloat("Coordinate axis scale", &m_worldCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show Mesh local coordinate axis").c_str(), &m_showMeshCoordinateAxis);
		if (m_showMeshCoordinateAxis)
		{
			ImGui::SliderFloat("Mesh coordinate axis scale", &m_meshCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show Mesh bounding boxes").c_str(), &m_showAllMeshBoundingBoxes);
		ImGui::Checkbox(std::format("Show MeshPrimitive bounding boxes").c_str(), &m_showAllMeshPrimitiveBoundingBoxes);

		ImGui::Checkbox(std::format("Show all vertex normals").c_str(), &m_showAllVertexNormals);
		if (m_showAllVertexNormals)
		{
			ImGui::SliderFloat("Vertex normals scale", &m_vertexNormalsScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show all camera frustums").c_str(), &m_showAllCameraFrustums);
		if (m_showAllCameraFrustums)
		{
			ImGui::SliderFloat("Camera coordinate axis scale", &m_cameraCoordinateAxisScale, 0.f, 20.f);
		}

		ImGui::Checkbox(std::format("Show all mesh wireframes").c_str(), &m_showAllWireframe);
	}
}