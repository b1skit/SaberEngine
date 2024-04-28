// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Core\Util\CastUtils.h"
#include "Config.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "Core\PerformanceTimer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace fr
{
	const NameID SceneManager::k_sceneRenderSystemNameID = en::NamedObject::ComputeIDFromName(k_sceneRenderSystemName);


	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<fr::SceneManager> instance = std::make_unique<fr::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager()
		: m_sceneData(nullptr)
		, m_sceneRenderSystemNameID(en::NamedObject::ComputeIDFromName(k_sceneRenderSystemName))
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		util::PerformanceTimer timer;
		timer.Start();

		// Load the scene:
		std::string sceneName;
		if (en::Config::Get()->TryGetValue<std::string>(core::configkeys::k_sceneNameKey, sceneName))
		{
			LOG("Creating scene \"%s\"", sceneName.c_str());
			m_sceneData = std::make_shared<fr::SceneData>(sceneName);
		}
		else
		{
			LOG("Creating an empty scene");
			m_sceneData = std::make_shared<fr::SceneData>("Empty Scene");
		}

		std::string sceneFilePath;
		en::Config::Get()->TryGetValue<std::string>(core::configkeys::k_sceneFilePathKey, sceneFilePath);

		const bool loadResult = m_sceneData->Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene from path \"%s\"", sceneFilePath.c_str());
		}
		else
		{
			LOG("\nSceneManager successfully loaded scene \"%s\" in %f seconds\n", 
				sceneFilePath.c_str(), timer.StopSec());
		}


		// Create a scene render system:
		class CreateSceneRenderSystemCommand
		{
		public:
			CreateSceneRenderSystemCommand() = default;
			~CreateSceneRenderSystemCommand() = default;

			static void Execute(void* cmdData)
			{
				CreateSceneRenderSystemCommand* cmdPtr = reinterpret_cast<CreateSceneRenderSystemCommand*>(cmdData);

				std::string pipelineFileName;
				if (en::Config::Get()->TryGetValue(core::configkeys::k_scenePipelineCmdLineArg, pipelineFileName) == false)
				{
					pipelineFileName = core::configkeys::k_defaultScenePipelineFileName;
				}

				re::RenderSystem const* sceneRenderSystem =
					re::RenderManager::Get()->CreateAddRenderSystem(k_sceneRenderSystemName, pipelineFileName);
			}

			static void Destroy(void* cmdData)
			{
				CreateSceneRenderSystemCommand* cmdPtr = reinterpret_cast<CreateSceneRenderSystemCommand*>(cmdData);
				cmdPtr->~CreateSceneRenderSystemCommand();
			}

		private:

		};
		re::RenderManager::Get()->EnqueueRenderCommand<CreateSceneRenderSystemCommand>();
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");


		// NOTE: OpenGL objects must be destroyed on the render thread, so we trigger them here by recording a command
		// to call SceneData::Destroy during RenderManager shutdown
		class DestroySceneDataRenderCommand
		{
		public:
			DestroySceneDataRenderCommand(std::shared_ptr<fr::SceneData> sceneData) : m_sceneData(sceneData) {}
			~DestroySceneDataRenderCommand() { m_sceneData = nullptr; }

			static void Execute(void* cmdData)
			{
				DestroySceneDataRenderCommand* cmdPtr = reinterpret_cast<DestroySceneDataRenderCommand*>(cmdData);
				cmdPtr->m_sceneData->Destroy();
			}

			static void Destroy(void* cmdData)
			{
				DestroySceneDataRenderCommand* cmdPtr = reinterpret_cast<DestroySceneDataRenderCommand*>(cmdData);
				cmdPtr->~DestroySceneDataRenderCommand();
			}
		private:
			std::shared_ptr<fr::SceneData> m_sceneData;
		};


		re::RenderManager::Get()->EnqueueRenderCommand<DestroySceneDataRenderCommand>(
			DestroySceneDataRenderCommand(std::move(m_sceneData)));
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// 
	}


	void SceneManager::ShowImGuiWindow(bool* show) const
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = en::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
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


