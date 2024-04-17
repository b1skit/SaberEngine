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

		void ShowImGuiWindow() override;


	private:
		// Mapping encapsulating Mesh's bounds and encapsulated MeshPrimitive bounds
		std::unordered_map<gr::RenderDataID, std::vector<gr::RenderDataID>> m_meshesToMeshPrimitiveBounds;
		std::unordered_map<gr::RenderDataID, gr::RenderDataID> m_meshPrimitivesToEncapsulatingMesh;

	private:
		// Cached frustum planes; (Re)computed when a camera is added/dirtied
		std::unordered_map<gr::Camera::View const, gr::Camera::Frustum> m_cachedFrustums;
		std::mutex m_cachedFrustumsMutex;

		bool m_cullingEnabled;
	};
}