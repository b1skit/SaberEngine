// � 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraControlComponent.h"
#include "EntityCommands.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "Load_Common.h"
#include "Load_GLTF.h"
#include "MeshConcept.h"
#include "SceneManager.h"

#include "Core/Config.h"
#include "Core/EventManager.h"
#include "Core/Inventory.h"

#include "Core/Definitions/EventKeys.h"

#include "Core/Host/PerformanceTimer.h"

#include "Core/Util/FileIOUtils.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/RenderSystem.h"


namespace pr
{
	SceneManager::SceneManager(pr::EntityManager* entityMgr)
		: m_entityManager(entityMgr)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		// Event subscriptions:
		core::EventManager::Subscribe(eventkey::FileImportRequest, this);
		core::EventManager::Subscribe(eventkey::SceneResetRequest, this);

		CreateDefaultSceneResources(); // Kick off async loading of mandatory assets

		Reset();

		// Create a scene render system:
		std::string pipelineFileName;
		if (!core::Config::TryGetValue(core::configkeys::k_scenePipelineCmdLineArg, pipelineFileName))
		{
			pipelineFileName = core::configkeys::k_defaultRenderPipelineFileName;
		}
		gr::RenderCommand::Enqueue<gr::CreateAddRenderSystem>(pipelineFileName);
	}

	void SceneManager::Reset()
	{
		LOG("SceneManager: Resetting scene");

		// Schedule initial scene setup:
		m_entityManager->EnqueueEntityCommand([this]()
			{
				// Create a scene bounds entity:
				pr::BoundsComponent::CreateSceneBoundsConcept(*m_entityManager);
				LOG("Created scene BoundsComponent");

				// Add an unbound camera controller to the scene:
				pr::CameraControlComponent::CreateCameraControlConcept(*m_entityManager, entt::null);
				LOG("Created unbound CameraControlComponent");
			});

		// Schedule creation of a default camera. Note: The ordering is important here, we schedule this 1st which
		// ensures if we import a camera after this point it will be activated
		m_entityManager->EnqueueEntityCommand<pr::SetMainCameraCommand>(
			load::CreateDefaultCamera(m_entityManager).m_owningEntity);

		core::InvPtr<re::Texture> defaultIBL =
			core::Inventory::Get<re::Texture>(core::configkeys::k_defaultEngineIBLFilePath);

		m_entityManager->EnqueueEntityCommand([this, defaultIBL]()
			{
				// Create an Ambient LightComponent, and make it active if requested:
				const bool ambientExists = m_entityManager->EntityExists<pr::LightComponent::AmbientIBLDeferredMarker>();
				if (!ambientExists)
				{
					const entt::entity ambientLight = pr::LightComponent::CreateDeferredAmbientLightConcept(
						*m_entityManager,
						defaultIBL->GetName().c_str(),
						defaultIBL);

					m_entityManager->EnqueueEntityCommand<pr::SetActiveAmbientLightCommand>(ambientLight);
				}
			});
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
		host::PerformanceTimer timer;
		timer.Start();

		bool success = false;

		std::string const& fileExtension = util::ExtractExtensionFromFilePath(filePath);
		if (fileExtension == "gltf" || fileExtension == "glb")
		{
			load::ImportGLTFFile(filePath); // Kicks off async loading
			success = true;
		}
		else if (fileExtension == "hdr") // Assume we want to create an IBL from the loaded texture
		{
			load::ImportIBL(filePath, load::IBLTextureFromFilePath::ActivationMode::Always);
			success = true;
		}

		if (success)
		{
			LOG("\nSceneManager scheduled file \"%s\" import in %f seconds\n", filePath.c_str(), timer.PeekSec());
		}
		else
		{
			LOG_ERROR("File path \"%s\" cannot be imported, it is not a recognized format", filePath.c_str());
		}

		timer.StopSec();
	}


	void SceneManager::CreateDefaultSceneResources()
	{
		LOG("Generating default resources...");

		load::ImportTexture(
			core::configkeys::k_defaultEngineIBLFilePath,
			re::Texture::k_errorTextureColor,
			re::Texture::Format::RGBA8_UNORM, // Fallback to something simple
			re::Texture::ColorSpace::Linear,
			re::Texture::MipMode::AllocateGenerate,
			true);

		load::GenerateDefaultGLTFMaterial();
	}


	void SceneManager::ShowImGuiWindow(bool* show) const
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = core::Config::GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene Manager";
		if (ImGui::Begin(k_panelTitle, show))
		{
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
					pr::LightComponent::ShowImGuiSpawnWindow(*m_entityManager);
				}
				break;
				case EntityType::Mesh:
				{
					pr::Mesh::ShowImGuiSpawnWindow(*m_entityManager);
				}
				break;
				default: SEAssertF("Invalid EntityType");
				}

				ImGui::Unindent();
			}
		}
		ImGui::End();
	}
}