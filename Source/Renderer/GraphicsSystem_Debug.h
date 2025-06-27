// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "GraphicsSystem.h"
#include "TransformRenderData.h"

#include "Core/AccessKey.h"


struct DebugData;

namespace fr
{
	class GraphicsService_Debug;
}

namespace gr
{
	struct DebugServiceData
	{
		// Colors for any/all coordinate axes
		glm::vec3 m_xAxisColor = glm::vec3(1.f, 0.f, 0.f);
		glm::vec3 m_yAxisColor = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 m_zAxisColor = glm::vec3(0.f, 0.f, 1.f);
		float m_axisOpacity = 0.5f;
		float m_axisScale = 0.2f;

		bool m_showWorldCoordinateAxis = false;

		// TODO: Move more features into here
	};


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

		static constexpr util::CHashKey k_viewBatchesDataInput = "ViewBatches";
		void RegisterInputs() override;

		void RegisterOutputs() override {};


	public:
		DebugGraphicsSystem(gr::GraphicsSystemManager*);
		~DebugGraphicsSystem();

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();

		void ShowImGuiWindow() override;


	private:
		void CreateBatches();

		DebugData PackDebugData() const;


	private: // Cached dependencies:
		ViewBatches const* m_viewBatches;


	private:
		std::shared_ptr<re::Stage> m_debugStage;
		std::shared_ptr<re::Stage> m_wireframeStage;

		re::BufferInput m_debugParams;
		bool m_isDirty; // Triggers m_debugParams recommit

		DebugServiceData m_serviceData;

		gr::BatchHandle m_worldCoordinateAxisBatch;

		bool m_showMeshCoordinateAxis = false;
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_meshCoordinateAxisBatches;

		bool m_showLightCoordinateAxis = false;
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_lightCoordinateAxisBatches;

		bool m_showSceneBoundingBox = false;
		glm::vec4 m_sceneBoundsColor = glm::vec4(1.f, 1.f, 1.f, 0.5f);
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_sceneBoundsBatches; // This is wasteful but convenient

		bool m_showAllMeshBounds = false;
		glm::vec4 m_meshBoundsColor = glm::vec4(1.f, 0.f, 0.f, 0.5f);
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_meshBoundsBatches;

		bool m_showAllMeshPrimitiveBounds = false;
		glm::vec4 m_meshPrimBoundsColor = glm::vec4(0.f, 1.f, 0.f, 0.5f);
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_meshPrimBoundsBatches;

		bool m_showAllLightBounds = false;
		glm::vec4 m_lightBoundsColor = glm::vec4(1.f, 1.f, 0.f, 0.5f);
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_lightBoundsBatches;
		
		bool m_showAllVertexNormals = false;
		float m_vertexNormalsScale = 0.2f;
		glm::vec4 m_normalsColor = glm::vec4(0.f, 0.f, 1.f, 0.5f);
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_vertexNormalBatches;

		bool m_showCameraFrustums = false;
		glm::vec4 m_cameraFrustumColor = glm::vec4(1.f, 1.f, 1.f, 0.5f);
		std::unordered_map<gr::RenderDataID, std::pair<gr::Camera::RenderData const*, gr::Transform::RenderData const*>> m_camerasToDebug;
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_cameraAxisBatches;
		std::unordered_map<gr::RenderDataID, std::vector<gr::BatchHandle>> m_cameraFrustumBatches;
		std::unordered_map<gr::RenderDataID, std::vector<re::BufferInput>> m_cameraFrustumTransformBuffers;

		bool m_showAllWireframe = false;
		glm::vec4 m_wireframeColor = glm::vec4(152/255.f, 1.f, 166/255.f, 0.5f);

		bool m_showDeferredLightWireframe = false;
		std::unordered_map<gr::RenderDataID, gr::BatchHandle> m_deferredLightWireframeBatches;

		bool m_showAllTransforms = false;
		std::unordered_map<gr::TransformID, gr::BatchHandle> m_transformAxisBatches;

		bool m_showParentChildLinks = false;
		std::unordered_map<gr::TransformID, gr::BatchHandle> m_transformParentChildLinkBatches;
		glm::vec4 m_parentColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
		glm::vec4 m_childColor = glm::vec4(0.f, 0.f, 0.f, 1.f);


	private:
		std::unordered_set<gr::RenderDataID> m_selectedRenderDataIDs; // If emtpy, render all IDs
		std::unordered_set<gr::TransformID> m_selectedTransformIDs;


	public: // Debug service interface:
		using AccessKey = accesscontrol::AccessKey<DebugGraphicsSystem, fr::GraphicsService_Debug>;

		void EnableWorldCoordinateAxis(AccessKey, bool show);
	};
}

