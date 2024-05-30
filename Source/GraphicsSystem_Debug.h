// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "GraphicsSystem.h"


namespace gr
{
	class DebugGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<DebugGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Debug"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(DebugGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(DebugGraphicsSystem, PreRender))
			);
		}

		void RegisterInputs() override {};
		void RegisterOutputs() override {};


	public:
		DebugGraphicsSystem(gr::GraphicsSystemManager*);

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&);

		void PreRender(DataDependencies const&);

		void ShowImGuiWindow() override;


	private:
		void CreateBatches();


	private:
		std::shared_ptr<re::RenderStage> m_debugLineStage;
		std::shared_ptr<re::RenderStage> m_debugTriangleStage;

		// Colors for any/all coordinate axis
		glm::vec3 m_xAxisColor = glm::vec3(1.f, 0.f, 0.f);
		glm::vec3 m_yAxisColor = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 m_zAxisColor = glm::vec3(0.f, 0.f, 1.f);

		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshPrimTransformBuffers;

		bool m_showWorldCoordinateAxis = false;
		float m_worldCoordinateAxisScale = 1.f;
		std::unique_ptr<re::Batch> m_worldCoordinateAxisBatch;

		bool m_showMeshCoordinateAxis = false;
		float m_meshCoordinateAxisScale = 1.f;
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_meshCoordinateAxisBatches;

		bool m_showLightCoordinateAxis = false;
		float m_lightCoordinateAxisScale = 1.f;
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_lightCoordinateAxisBatches;
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_lightCoordinateAxisTransformBuffers;

		bool m_showSceneBoundingBox = false;
		glm::vec3 m_sceneBoundsColor = glm::vec3(1.f, 0.4f, 0.f);
		std::unique_ptr<re::Batch> m_sceneBoundsBatch;
		std::shared_ptr<re::Buffer> m_sceneBoundsTransformBuffer;

		bool m_showAllMeshBoundingBoxes = false;
		glm::vec3 m_meshBoundsColor = glm::vec3(1.f, 0.f, 0.f);
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_meshBoundingBoxBatches;
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshBoundingBoxBuffers;

		bool m_showAllMeshPrimitiveBoundingBoxes = false;
		glm::vec3 m_meshPrimitiveBoundsColor = glm::vec3(0.f, 1.f, 0.f);
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_meshPrimBoundingBoxBatches;
		
		bool m_showAllVertexNormals = false;
		float m_vertexNormalsScale = 1.f;
		glm::vec3 m_normalsColor = glm::vec3(0.f, 0.f, 1.f);
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_vertexNormalBatches;

		bool m_showCameraFrustums = false;
		glm::vec3 m_cameraFrustumColor = glm::vec3(1.f, 1.f, 1.f);
		float m_cameraCoordinateAxisScale = 1.f;
		std::unordered_map<gr::RenderDataID, std::pair<gr::Camera::RenderData const*, gr::Transform::RenderData const*>> m_camerasToDebug;
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_cameraAxisBatches;
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_cameraAxisTransformBuffers;
		std::unordered_map<gr::RenderDataID, std::vector<std::unique_ptr<re::Batch>>> m_cameraFrustumBatches;
		std::unordered_map<gr::RenderDataID, std::vector<std::shared_ptr<re::Buffer>>> m_cameraFrustumTransformBuffers;

		bool m_showAllWireframe = false;
		glm::vec3 m_wireframeColor = glm::vec3(152/255.f, 1.f, 166/255.f);
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_wireframeBatches;

		bool m_showDeferredLightWireframe = false;
		glm::vec3 m_deferredLightwireframeColor = glm::vec3(1.f, 1.f, 0.f);
		std::unordered_map<gr::RenderDataID, std::unique_ptr<re::Batch>> m_deferredLightWireframeBatches;
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_deferredLightWireframeTransformBuffers;

		std::unordered_set<gr::RenderDataID> m_selectedRenderDataIDs; // If emtpy, render all IDs
	};
}

