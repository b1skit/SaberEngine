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
				// Note: No INIT_PIPELINE functionality for Culling
				PRE_RENDER(PRE_RENDER_FN(CullingGraphicsSystem, PreRender))
			);
		}

		void RegisterTextureInputs() override {};
		void RegisterTextureOutputs() override {};


	public:
		CullingGraphicsSystem(gr::GraphicsSystemManager*);
		~CullingGraphicsSystem() override = default;

		void InitPipeline();

		void PreRender();

		std::vector<gr::RenderDataID> const& GetVisibleRenderDataIDs(gr::Camera::View const&) const;
		std::vector<gr::RenderDataID> GetVisibleRenderDataIDs(std::vector<gr::Camera::View> const&) const;
		
		std::vector<gr::RenderDataID> const& GetVisiblePointLights() const; // For the currently active camera
		std::vector<gr::RenderDataID> const& GetVisibleSpotLights() const;

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
		std::map<gr::Camera::View const, std::vector<gr::RenderDataID>> m_viewToVisibleIDs;
		std::mutex m_viewToVisibleIDsMutex;

		// A list of light RenderDataIDs visible to the main camera
		std::vector<gr::RenderDataID> m_visiblePointLightIDs;
		std::vector<gr::RenderDataID> m_visibleSpotLightIDs;
		std::mutex m_visibleLightsMutex;

		bool m_cullingEnabled;
	};


	inline std::vector<gr::RenderDataID> const& CullingGraphicsSystem::GetVisibleRenderDataIDs(
		gr::Camera::View const& view) const
	{
		SEAssert(m_viewToVisibleIDs.contains(view), "Camera with the given RenderDataID not found");

		return m_viewToVisibleIDs.at(view);
	}


	inline std::vector<gr::RenderDataID> const& CullingGraphicsSystem::GetVisiblePointLights() const
	{
		return m_visiblePointLightIDs;
	}


	inline std::vector<gr::RenderDataID> const& CullingGraphicsSystem::GetVisibleSpotLights() const
	{
		return m_visibleSpotLightIDs;
	}
}