// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraControlComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "Load_Common.h"
#include "Load_GLTF.h"
#include "MeshConcept.h"
#include "SceneManager.h"

#include "Core/Config.h"
#include "Core/EventManager.h"
#include "Core/PerformanceTimer.h"

#include "Renderer/RenderManager.h"


namespace fr
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<fr::SceneManager> instance = std::make_unique<fr::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager()
		: m_inventory(nullptr)
		, m_hasCreatedScene(false)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		SEAssert(m_inventory, "Inventory is null. This dependency must be injected immediately after creation");

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(eventkey::FileImportRequest, this);
		core::EventManager::Get()->Subscribe(eventkey::SceneResetRequest, this);

		Reset();

		// Create a scene render system:
		re::RenderManager::Get()->EnqueueRenderCommand([]()
			{
				std::string pipelineFileName;
				if (!core::Config::Get()->TryGetValue(core::configkeys::k_scenePipelineCmdLineArg, pipelineFileName))
				{
					pipelineFileName = core::configkeys::k_defaultScenePipelineFileName;
				}

				re::RenderSystem const* sceneRenderSystem =
					re::RenderManager::Get()->CreateAddRenderSystem(k_sceneRenderSystemName, pipelineFileName);
			});
	}

	void SceneManager::Reset()
	{
		LOG("SceneManager: Resetting scene");

		const bool prevHasCreatedScene = m_hasCreatedScene.load();
		m_hasCreatedScene.store(false);

		CreateDefaultSceneResources(); // Kick off async loading of mandatory assets

		// Initial scene setup:
		fr::EntityManager* em = fr::EntityManager::Get();
		em->EnqueueEntityCommand([em]()
			{
				// Create a scene bounds entity:
				fr::BoundsComponent::CreateSceneBoundsConcept(*em);
				LOG("Created scene BoundsComponent");

				// Add an unbound camera controller to the scene:
				fr::CameraControlComponent::CreateCameraControlConcept(*em, entt::null);
				LOG("Created unbound CameraControlComponent");
			});

		// If we're reseting an existing scene, recreate the default IBL
		if (prevHasCreatedScene)
		{
			core::InvPtr<re::Texture> defaultIBL = 
				m_inventory->Get<re::Texture>(core::configkeys::k_defaultEngineIBLFilePath);

			em->EnqueueEntityCommand([em, defaultIBL]()
				{
					// Create an Ambient LightComponent, and make it active if requested:
					const entt::entity ambientLight = fr::LightComponent::CreateDeferredAmbientLightConcept(
						*em,
						defaultIBL->GetName().c_str(),
						defaultIBL);

					em->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(ambientLight);
				});
		}

		// Note: We kick off the loading flow with an empty string to ensure a default camera is created
		ImportFile("");
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		//
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();
	}


	void SceneManager::HandleEvents()
	{
		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_eventKey)
			{
			case eventkey::FileImportRequest:
			{
				std::string const& filepath = std::get<std::string>(eventInfo.m_data);
				ImportFile(filepath);
			}
			break;
			case eventkey::SceneResetRequest:
			{
				Reset();
			}
			break;
			default:
				break;
			}
		}
	}


	void SceneManager::ImportFile(std::string const& filePath)
	{
		util::PerformanceTimer timer;
		timer.Start();

		load::ImportGLTFFile(m_inventory, filePath); // Kicks off async loading

		LOG("\nSceneManager scheduled file \"%s\" import in %f seconds\n", filePath.c_str(), timer.StopSec());
	}


	void SceneManager::NotifyLoadComplete()
	{
		SceneManager* sceneMgr = fr::SceneManager::Get();

		bool expected = false;
		if (sceneMgr->m_hasCreatedScene.compare_exchange_strong(expected, true))
		{
			LOG("SceneManager: Initial scene created");

			core::EventManager::Get()->Notify(core::EventManager::EventInfo{
				.m_eventKey = eventkey::SceneCreated });
		}
	}


	void SceneManager::CreateDefaultSceneResources()
	{
		LOG("Generating default resources...");

		load::ImportIBL(
			m_inventory,
			core::configkeys::k_defaultEngineIBLFilePath,
			load::IBLTextureFromFilePath::ActivationMode::IfNoneExists,
			true);

		load::GenerateDefaultGLTFMaterial(m_inventory);
	}


	void SceneManager::ShowImGuiWindow(bool* show) const
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene Manager";
		ImGui::Begin(k_panelTitle, show);

		if (ImGui::CollapsingHeader("Spawn Entities", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();

			enum EntityType : uint8_t
			{
				Light,
				Mesh,

				EntityType_Count
			};
			constexpr std::array<char const*, EntityType::EntityType_Count> k_entityTypeNames = {
				"Light",
				"Mesh"
			};
			static_assert(k_entityTypeNames.size() == EntityType::EntityType_Count);

			constexpr ImGuiComboFlags k_comboFlags = 0;

			static EntityType s_selectedEntityTypeIdx = static_cast<EntityType>(0);
			const EntityType currentSelectedEntityTypeIdx = s_selectedEntityTypeIdx;
			if (ImGui::BeginCombo("Entity type", k_entityTypeNames[s_selectedEntityTypeIdx], k_comboFlags))
			{
				for (uint8_t comboIdx = 0; comboIdx < k_entityTypeNames.size(); comboIdx++)
				{
					const bool isSelected = comboIdx == s_selectedEntityTypeIdx;
					if (ImGui::Selectable(k_entityTypeNames[comboIdx], isSelected))
					{
						s_selectedEntityTypeIdx = static_cast<EntityType>(comboIdx);
					}

					// Set the initial focus:
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			ImGui::Separator();


			switch (s_selectedEntityTypeIdx)
			{
			case EntityType::Light:
			{
				fr::LightComponent::ShowImGuiSpawnWindow();
			}
			break;
			case EntityType::Mesh:
			{
				fr::Mesh::ShowImGuiSpawnWindow();
			}
			break;
			default: SEAssertF("Invalid EntityType");
			}

			ImGui::Unindent();
		}

		ImGui::End();
	}
}