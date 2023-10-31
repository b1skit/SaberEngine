// © 2023 Adam Badke. All rights reserved.
#include "ConfigKeys.h"
#include "GraphicsSystem_Debug.h"
#include "SceneManager.h"


namespace
{
	re::Batch BuildAxisBatch(float axisScale, glm::vec3 xAxisColor, glm::vec3 yAxisColor, glm::vec3 zAxisColor)
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
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&axisPositions)));

		std::shared_ptr<re::VertexStream> axisColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&axisColors)));

		std::vector<re::VertexStream*> vertexStreams;
		vertexStreams.resize(gr::MeshPrimitive::Slot_Count, nullptr); // position, color

		vertexStreams[gr::MeshPrimitive::Slot::Position] = axisPositionStream.get();
		vertexStreams[gr::MeshPrimitive::Slot::Color] = axisColorStream.get();

		re::Batch::GraphicsParams axisBatchGraphicsParams{};
		axisBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		axisBatchGraphicsParams.m_numInstances = 1;
		axisBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		memcpy(axisBatchGraphicsParams.m_vertexStreams.data(),
			vertexStreams.data(),
			axisBatchGraphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, axisBatchGraphicsParams);
	}


	re::Batch BuildBoundingBoxBatch(gr::Bounds const& bounds, glm::vec3 boxColor)
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

		std::vector<glm::vec3> boxPositions = { 
			// Front face:
			a, c,
			c, d,
			d, b,
			b, a,

			// Left face:
			e, g,
			g, c,
			c, a,
			a, e,

			// Right face:
			f, b,
			b, d,
			d, h,
			h, f,

			// Back face:
			f, e,
			e, g, 
			g, h, 
			h, f,

			// Top face:
			e, a, 
			a, b, 
			b, f, 
			f, e,

			// Bottom face:
			d, c,
			c, g, 
			g, h, 
			h, d
		};

		const glm::vec4 boxColorVec4 = glm::vec4(boxColor, 1.f);
		std::vector<glm::vec4> boxColors = std::vector<glm::vec4>(boxPositions.size(), boxColorVec4);


		std::shared_ptr<re::VertexStream> boxPositionsStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&boxPositions)));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&boxColors)));

		std::vector<re::VertexStream*> vertexStreams;
		vertexStreams.resize(gr::MeshPrimitive::Slot_Count, nullptr); // position, color

		vertexStreams[gr::MeshPrimitive::Slot::Position] = boxPositionsStream.get();
		vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		memcpy(boundingBoxBatchGraphicsParams.m_vertexStreams.data(),
			vertexStreams.data(),
			boundingBoxBatchGraphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, boundingBoxBatchGraphicsParams);
	}

	re::Batch BuildVertexNormalsBatch(gr::MeshPrimitive const* meshPrimitive, float scale, glm::vec3 normalColor)
	{
		re::VertexStream const* positionStream = meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position);
		re::VertexStream const* normalStream = meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Normal);
		
		// For now, just support indexed geometry
		SEAssert("?????????????????", positionStream && normalStream);

		std::vector<glm::vec3> linePositions;

		SEAssert("Unexpected position or normal data", 
			positionStream->GetDataType() == re::VertexStream::DataType::Float && 
			positionStream->GetNumComponents() == 3 &&
			normalStream->GetDataType() == re::VertexStream::DataType::Float &&
			normalStream->GetNumComponents() == 3);

		glm::vec3 const* positionData = static_cast<glm::vec3 const*>(positionStream->GetData());
		glm::vec3 const* normalData = static_cast<glm::vec3 const*>(normalStream->GetData());
		for (uint32_t elementIdx = 0; elementIdx < positionStream->GetNumElements(); elementIdx++)
		{
			linePositions.emplace_back(positionData[elementIdx]);
			linePositions.emplace_back(positionData[elementIdx] + normalData[elementIdx] * scale);
		}
		
		const glm::vec4 normalColorVec4 = glm::vec4(normalColor, 1.f);
		std::vector<glm::vec4> normalColors = std::vector<glm::vec4>(linePositions.size(), normalColorVec4);
		
		std::vector<re::VertexStream*> vertexStreams;
		vertexStreams.resize(gr::MeshPrimitive::Slot_Count, nullptr); // position, color

		std::shared_ptr<re::VertexStream> normalPositionsStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			3, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&linePositions)));

		std::shared_ptr<re::VertexStream> boxColorStream = re::VertexStream::Create(
			re::VertexStream::Lifetime::SingleFrame,
			re::VertexStream::StreamType::Vertex,
			4, // numComponents per element
			re::VertexStream::DataType::Float,
			re::VertexStream::Normalize::False,
			std::move(*reinterpret_cast<std::vector<uint8_t>*>(&normalColors)));

		vertexStreams[gr::MeshPrimitive::Slot::Position] = normalPositionsStream.get();
		vertexStreams[gr::MeshPrimitive::Slot::Color] = boxColorStream.get();

		re::Batch::GraphicsParams boundingBoxBatchGraphicsParams{};
		boundingBoxBatchGraphicsParams.m_batchGeometryMode = re::Batch::GeometryMode::ArrayInstanced;
		boundingBoxBatchGraphicsParams.m_numInstances = 1;
		boundingBoxBatchGraphicsParams.m_batchTopologyMode = gr::MeshPrimitive::TopologyMode::LineList;

		memcpy(boundingBoxBatchGraphicsParams.m_vertexStreams.data(),
			vertexStreams.data(),
			boundingBoxBatchGraphicsParams.m_vertexStreams.size() * sizeof(re::VertexStream const*));

		return re::Batch(re::Batch::Lifetime::SingleFrame, nullptr, boundingBoxBatchGraphicsParams);
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
			m_showAllVertexNormals)
		{
			for (auto const& mesh : en::SceneManager::GetSceneData()->GetMeshes())
			{
				std::shared_ptr<re::ParameterBlock> meshTransformPB =
					gr::Mesh::CreateInstancedMeshParamsData(&mesh->GetTransform()->GetGlobalMatrix(gr::Transform::TRS), nullptr);

				if (m_showAllMeshBoundingBoxes)
				{
					re::Batch boundingBoxBatch = BuildBoundingBoxBatch(mesh->GetBounds(), m_meshBoundsColor);
					boundingBoxBatch.SetParameterBlock(meshTransformPB);
					m_debugStage->AddBatch(boundingBoxBatch);
				}
				if (m_showAllMeshPrimitiveBoundingBoxes || m_showAllVertexNormals)
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
					}
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
	}
}