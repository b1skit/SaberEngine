// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "CameraRenderData.h"
#include "GraphicsSystem.h"


namespace gr
{
	class CullingGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<CullingGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Culling"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(CullingGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(CullingGraphicsSystem, PreRender))
			);
		}

		void RegisterInputs() override { /*No inputs*/ };

		static constexpr char const* k_cullingOutput = "ViewCullingResults";
		static constexpr char const* k_pointLightCullingOutput = "PointLightCullingResults";
		static constexpr char const* k_spotLightCullingOutput = "SpotLightCullingResults";
		void RegisterOutputs() override;


	public:
		CullingGraphicsSystem(gr::GraphicsSystemManager*);
		~CullingGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&);
		void PreRender(DataDependencies const&);

		void ShowImGuiWindow() override;


	private:
		// Mapping encapsulating Mesh's bounds and encapsulated MeshPrimitive bounds
		std::unordered_map<gr::RenderDataID, std::vector<gr::RenderDataID>> m_meshesToMeshPrimitiveBounds;
		std::unordered_map<gr::RenderDataID, gr::RenderDataID> m_meshPrimitivesToEncapsulatingMesh;

	private:
		// Cached frustum planes; (Re)computed when a camera is added/dirtied
		std::unordered_map<gr::Camera::View const, gr::Camera::Frustum> m_cachedFrustums;
		std::mutex m_cachedFrustumsMutex;

		// Mapping Camera RenderDataIDs to a list of RenderDataIDs visible after culling
		std::unordered_map<gr::Camera::View const, std::vector<gr::RenderDataID>> m_viewToVisibleIDs;
		std::mutex m_viewToVisibleIDsMutex;

		// A list of light RenderDataIDs visible to the main camera
		std::vector<gr::RenderDataID> m_visiblePointLightIDs;
		std::vector<gr::RenderDataID> m_visibleSpotLightIDs;
		std::mutex m_visibleLightsMutex;

		bool m_cullingEnabled;
	};
}