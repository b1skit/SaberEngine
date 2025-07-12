// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "IGraphicsService.h"

#include "Renderer/GraphicsSystem_Debug.h"


namespace pr
{
	class GraphicsService_Debug final : public virtual IGraphicsService
	{
	public:
		GraphicsService_Debug() = default;

		void DoInitialize() override;


	public:
		bool IsWorldCoordinateAxisVisible() const;
		void EnableWorldCoordinateAxis(bool show);


	public:
		void PopulateImGuiMenu();


	private: // Static state shared by all owners of a service instance
		static gr::DebugServiceData s_debugData;
		static std::shared_mutex s_debugDataMutex;


	private:
		static gr::DebugGraphicsSystem* s_debugGraphicsSystem;
	};
}