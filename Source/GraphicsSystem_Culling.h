// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "CameraRenderData.h"
#include "GraphicsSystem.h"


namespace gr
{
	class CullingGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		CullingGraphicsSystem(gr::GraphicsSystemManager*);
		~CullingGraphicsSystem() override = default;

		void Create();

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		std::vector<gr::RenderDataID> const& GetVisibleRenderDataIDs(gr::Camera::View const&) const;

		void ShowImGuiWindow() override;


	private:
		void CreateBatches() override { /*Do nothing*/ };


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

		bool m_cullingEnabled;
	};


	inline std::shared_ptr<re::TextureTargetSet const> CullingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		SEAssertF("No targets are (currently) emitted during culling");
		return nullptr;
	}


	inline std::vector<gr::RenderDataID> const& CullingGraphicsSystem::GetVisibleRenderDataIDs(
		gr::Camera::View const& view) const
	{
		SEAssert(m_viewToVisibleIDs.contains(view), "Camera with the given RenderDataID not found");

		return m_viewToVisibleIDs.at(view);
	}
}