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

#include "Core/Definitions/EventKeys.h"

#include "Core/Host/PerformanceTimer.h"

#include "Core/Util/FileIOUtils.h"

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
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		SEAssert(m_inventory, "Inventory is null. This dependency must be injected immediately after creation");

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(eventkey::FileImportRequest, this);
		core::EventManager::Get()->Subscribe(eventkey::SceneResetRequest, this);

		CreateDefaultSceneResources(); // Kick off async loading of mandatory assets

		Reset();

		// Create a scene render system:
		re::RenderManager::Get()->EnqueueRenderCommand([]()
			{
				std::string pipelineFileName;
				if (!core::Config::Get()->TryGetValue(core::configkeys::k_scenePipelineCmdLineArg, pipelineFileName))
				{
					pipelineFileName = core::configkeys::k_defaultRenderPipelineFileName;
				}

				gr::RenderSystem const* sceneRenderSystem = 
					re::RenderManager::Get()->CreateAddRenderSystem(pipelineFileName);
			});
	}

	void SceneManager::Reset()
	{
		LOG("SceneManager: Resetting scene");

		// Schedule initial scene setup:
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

		// Schedule creation of a default camera. Note: The ordering is important here, we schedule this 1st which
		// ensures if we import a camera after this point it will be activated
		fr::EntityManager::Get()->EnqueueEntityCommand<fr::SetMainCameraCommand>(
			load::CreateDefaultCamera(fr::EntityManager::Get()).m_owningEntity);

		core::InvPtr<re::Texture> defaultIBL =
			m_inventory->Get<re::Texture>(core::configkeys::k_defaultEngineIBLFilePath);

		em->EnqueueEntityCommand([em, defaultIBL]()
			{
				// Create an Ambient LightComponent, and make it active if requested:
				const bool ambientExists = em->EntityExists<fr::LightComponent::AmbientIBLDeferredMarker>();
				if (!ambientExists)
				{
					const entt::entity ambientLight = fr::LightComponent::CreateDeferredAmbientLightConcept(
						*em,
						defaultIBL->GetName().c_str(),
						defaultIBL);

					em->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(ambientLight);
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
			load::ImportGLTFFile(m_inventory, filePath); // Kicks off async loading
			success = true;
		}
		else if (fileExtension == "hdr") // Assume we want to create an IBL from the loaded texture
		{
			load::ImportIBL(m_inventory, filePath, load::IBLTextureFromFilePath::ActivationMode::Always);
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
			m_inventory,
			core::configkeys::k_defaultEngineIBLFilePath,
			re::Texture::k_errorTextureColor,
			re::Texture::Format::RGBA8_UNORM, // Fallback to something simple
			re::Texture::ColorSpace::Linear,
			re::Texture::MipMode::AllocateGenerate,
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