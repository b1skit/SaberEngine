// © 2025 Adam Badke. All rights reserved.
#include "GraphicsService_Culling.h"

#include "CameraComponent.h"
#include "EntityManager.h"
#include "NameComponent.h"
#include "RenderDataComponent.h"

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/AccessKey.h"
#include "Core/SystemLocator.h"

#include "Renderer/GraphicsSystem_Culling.h"


namespace fr
{
	gr::CullingServiceData CullingGraphicsService::s_cullingData{};

	std::shared_mutex CullingGraphicsService::s_cullingDataMutex;

	gr::CullingGraphicsSystem* CullingGraphicsService::s_cullingGraphicsSystem = nullptr;


	// ---


	void CullingGraphicsService::DoInitialize()
	{
		if (s_cullingGraphicsSystem == nullptr)
		{
			s_cullingGraphicsSystem =
				core::SystemLocator::Get<gr::CullingGraphicsSystem>(ACCESS_KEY(gr::CullingGraphicsSystem::AccessKey));

			// Optionally start with culling disabled by the command line
			if (core::Config::Get()->KeyExists(core::configkeys::k_disableCullingCmdLineArg))
			{
				{
					std::unique_lock<std::shared_mutex> lock(s_cullingDataMutex);

					s_cullingData.m_cullingEnabled = false;
				}
			}

			EnableCulling(s_cullingData.m_cullingEnabled);
		}
	}


	bool CullingGraphicsService::IsCullingEnabled() const
	{
		if (s_cullingGraphicsSystem)
		{
			std::shared_lock<std::shared_mutex> lock(s_cullingDataMutex);

			return s_cullingData.m_cullingEnabled;
		}
		return false;
	}


	void CullingGraphicsService::EnableCulling(bool isEnabled)
	{
		if (s_cullingGraphicsSystem)
		{
			std::unique_lock<std::shared_mutex> lock(s_cullingDataMutex);

			EnqueueServiceCommand([this, isEnabled]()
				{
					s_cullingGraphicsSystem->EnableCulling(
						ACCESS_KEY(gr::CullingGraphicsSystem::AccessKey),
						s_cullingData.m_cullingEnabled);
				});

			s_cullingData.m_cullingEnabled = isEnabled;
		}
		else
		{
			LOG_ERROR("CullingGraphicsService has not been bound to the CullingGraphicsSystem");
		}
	}


	void CullingGraphicsService::SetCullingDebugOverride(gr::RenderDataID overrideCameraID)
	{
		if (s_cullingGraphicsSystem)
		{
			std::unique_lock<std::shared_mutex> lock(s_cullingDataMutex);

			EnqueueServiceCommand([this, overrideCameraID]()
				{
					s_cullingGraphicsSystem->SetDebugCameraOverride(
						ACCESS_KEY(gr::CullingGraphicsSystem::AccessKey), overrideCameraID);
				});
		}
		else
		{
			LOG_ERROR("CullingGraphicsService has not been bound to the CullingGraphicsSystem");
		}

		s_cullingData.m_debugCameraOverrideID = overrideCameraID;
	}


	void CullingGraphicsService::PopulateImGuiMenu()
	{
		bool cullingEnabled = IsCullingEnabled();
		if (ImGui::Checkbox("Enable culling", &cullingEnabled))
		{
			EnableCulling(cullingEnabled);
		}

		if (ImGui::BeginMenu("Culling override"))
		{
			std::vector<std::pair<std::string, gr::RenderDataID>> const& cameras =
				fr::EntityManager::Get()->QueryRegistry<fr::CameraComponent, fr::NameComponent, fr::RenderDataComponent>(
					[](auto const& view) -> std::vector<std::pair<std::string, gr::RenderDataID>>
					{
						std::vector<std::pair<std::string, gr::RenderDataID>>  camIDs;

						// Add the "Disabled" option as the first entry:
						camIDs.emplace_back("Disabled", gr::k_invalidRenderDataID);

						for (auto const& [entity, camCmpt, nameCmpt, renderDataCmpt] : view.each())
						{
							camIDs.emplace_back(nameCmpt.GetName(), renderDataCmpt.GetRenderDataID());
						}
						return camIDs;
					});

			for (auto const& cam : cameras)
			{
				if (ImGui::MenuItem(cam.first.c_str()))
				{
					SetCullingDebugOverride(cam.second);
				}
			}

			ImGui::EndMenu();
		}
	}
}