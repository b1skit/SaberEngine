// © 2025 Adam Badke. All rights reserved.
#include "GraphicsService_Debug.h"

#include "Core/Logger.h"
#include "Core/SystemLocator.h"


namespace pr
{
	gr::DebugServiceData GraphicsService_Debug::s_debugData{};
	std::shared_mutex GraphicsService_Debug::s_debugDataMutex;

	gr::DebugGraphicsSystem* GraphicsService_Debug::s_debugGraphicsSystem = nullptr;


	// ---


	void GraphicsService_Debug::DoInitialize()
	{
		if (s_debugGraphicsSystem == nullptr)
		{
			s_debugGraphicsSystem =
				core::SystemLocator::Get<gr::DebugGraphicsSystem>(ACCESS_KEY(gr::DebugGraphicsSystem::AccessKey));
		}
	}


	bool GraphicsService_Debug::IsWorldCoordinateAxisVisible() const
	{
		if (s_debugGraphicsSystem)
		{
			std::shared_lock<std::shared_mutex> lock(s_debugDataMutex);
			return s_debugData.m_showWorldCoordinateAxis;
		}
		return false;
	}


	void GraphicsService_Debug::EnableWorldCoordinateAxis(bool show)
	{
		{
			std::unique_lock<std::shared_mutex> lock(s_debugDataMutex);
			s_debugData.m_showWorldCoordinateAxis = show;
		}

		if (s_debugGraphicsSystem)
		{
			EnqueueServiceCommand([show]()
				{
					s_debugGraphicsSystem->EnableWorldCoordinateAxis(ACCESS_KEY(gr::DebugGraphicsSystem::AccessKey), show);
				});
		}
		else
		{
			LOG_ERROR("GraphicsService_Debug has not been bound to the DebugGraphicsSystem");
		}
	}


	void GraphicsService_Debug::PopulateImGuiMenu()
	{
		bool showWorldCSAxis = IsWorldCoordinateAxisVisible();
		if (ImGui::Checkbox("Show world origin", &showWorldCSAxis))
		{
			EnableWorldCoordinateAxis(showWorldCSAxis);
		}
	}
}