// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "IGraphicsService.h"

#include "Renderer/GraphicsSystem_Culling.h"
#include "Renderer/RenderObjectIDs.h"


namespace fr
{
	class CullingGraphicsService : public virtual IGraphicsService
	{
	public:
		CullingGraphicsService() = default;

		void DoInitialize() override;


	public:
		bool IsCullingEnabled() const;
		void EnableCulling(bool isEnabled);

		// View the culling results for a specific camera, rendered via the currently active camera
		void SetCullingDebugOverride(gr::RenderDataID overrideCameraID); // gr::k_invalidRenderDataID disables override


	public:
		void PopulateImGuiMenu();


	private: // Static state shared by all owners of a service instance
		static gr::CullingServiceData s_cullingData;
		static std::shared_mutex s_cullingDataMutex;


	private:
		static gr::CullingGraphicsSystem* s_cullingGraphicsSystem;
	};
}