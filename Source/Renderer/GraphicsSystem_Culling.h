// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "CameraRenderData.h"
#include "GraphicsSystem.h"
#include "RenderObjectIDs.h"

#include "Core/AccessKey.h"


namespace pr
{
	class CullingGraphicsService;
}
namespace gr
{
	struct CullingServiceData
	{
		gr::RenderDataID m_debugCameraOverrideID = gr::k_invalidRenderDataID;
		bool m_cullingEnabled = true;
	};


	// ---


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

		static constexpr util::CHashKey k_cullingOutput = "ViewCullingResults";
		static constexpr util::CHashKey k_pointLightCullingOutput = "PointLightCullingResults";
		static constexpr util::CHashKey k_spotLightCullingOutput = "SpotLightCullingResults";
		void RegisterOutputs() override;


	public:
		CullingGraphicsSystem(gr::GraphicsSystemManager*);
		~CullingGraphicsSystem() override;

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();

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
		ViewCullingResults m_viewToVisibleIDs;
		std::mutex m_viewToVisibleIDsMutex;

		// A list of light RenderDataIDs visible to the main camera
		std::vector<gr::RenderDataID> m_visiblePointLightIDs;
		std::vector<gr::RenderDataID> m_visibleSpotLightIDs;
		std::mutex m_visibleLightsMutex;


	public: // Culling service interface:
		using AccessKey = accesscontrol::AccessKey<CullingGraphicsSystem, pr::CullingGraphicsService>;

		void EnableCulling(AccessKey, bool isEnabled);

		// Enable culling debug override for a specific camera, rendered via the currently active camera.
		// Disable by passing gr::k_invalidRenderDataID				
		void SetDebugCameraOverride(AccessKey, gr::RenderDataID);


	private:
		CullingServiceData m_cullingServiceData;
	};
}