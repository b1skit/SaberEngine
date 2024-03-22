// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CastUtils.h"
#include "Config.h"
#include "EntityManager.h"
#include "FileIOUtils.h"
#include "LightComponent.h"
#include "PerformanceTimer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace fr
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<fr::SceneManager> instance = std::make_unique<fr::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager()
		: m_sceneData(nullptr)
	{
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		util::PerformanceTimer timer;
		timer.Start();

		// Load the scene:
		std::string sceneName;
		if (en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneNameKey, sceneName))
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
		en::Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_sceneFilePathKey, sceneFilePath);

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

		static const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene Manager";
		ImGui::Begin(k_panelTitle, show);

		if (ImGui::CollapsingHeader("Spawn Entities"))
		{
			ImGui::Indent();

			enum EntityToSpawn : uint8_t
			{
				AmbientLight,
				DirectionalLight,
				PointLight,

				EntityToSpawn_Count
			};
			std::array<char const*, EntityToSpawn::EntityToSpawn_Count> arr = {
				"Ambient Light",
				"Directional Light",
				"Point Light",
			};

			const ImGuiComboFlags flags = 0;

			static uint8_t s_entitySelectionIdx = 0;
			const uint8_t currentSelectionIdx = s_entitySelectionIdx;
			if (ImGui::BeginCombo("Entity type", arr[s_entitySelectionIdx], flags))
			{
				for (uint8_t comboIdx = 0; comboIdx < arr.size(); comboIdx++)
				{
					const bool isSelected = comboIdx == s_entitySelectionIdx;
					if (ImGui::Selectable(arr[comboIdx], isSelected))
					{
						s_entitySelectionIdx = comboIdx;
					}

					// Set the initial focus:
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			union SpawnParams
			{
				SpawnParams() { memset(this, 0, sizeof(SpawnParams)); }
				~SpawnParams() {}

				struct AmbientLightSpawnParams
				{
					std::string m_filepath;
				} m_ambientLightSpawnParams;

				struct PunctualLightSpawnParams
				{
					bool m_attachShadow;
					glm::vec4 m_colorIntensity;
				} m_punctualLightSpawnParams;
			};
			static std::unique_ptr<SpawnParams> s_spawnParams = std::make_unique<SpawnParams>();
			auto InitializeSpawnParams = [](std::unique_ptr<SpawnParams>& spawnParams)
				{
					spawnParams = std::make_unique<SpawnParams>();
				};

			// If the selection has changed, re-initialize the spawn parameters:
			if (s_spawnParams == nullptr || s_entitySelectionIdx != currentSelectionIdx)
			{
				InitializeSpawnParams(s_spawnParams);
			}

			// Display type-specific spawn options
			switch (static_cast<EntityToSpawn>(s_entitySelectionIdx))
			{
			case EntityToSpawn::AmbientLight:
			{
				std::vector<std::string> const& iblHDRFiles = util::GetDirectoryFilenameContents(
					en::Config::Get()->GetValue<std::string>(en::ConfigKeys::k_sceneIBLDirKey).c_str(), ".hdr");

				static size_t selectedFileIdx = 0;
				if (ImGui::BeginListBox("Selected source IBL HDR File"))
				{
					for (size_t i = 0; i < iblHDRFiles.size(); i++)
					{
						const bool isSelected = selectedFileIdx == i;
						if (ImGui::Selectable(iblHDRFiles[i].c_str(), isSelected))
						{
							selectedFileIdx = i;
						}
						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
							s_spawnParams->m_ambientLightSpawnParams.m_filepath = iblHDRFiles[selectedFileIdx];
						}
					}
					ImGui::EndListBox();
				}
			}
			break;
			case EntityToSpawn::DirectionalLight:
			case EntityToSpawn::PointLight:
			{
				ImGui::Checkbox("Attach shadow map", &s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
				ImGui::ColorEdit3("Color",
					&s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity.r,
					ImGuiColorEditFlags_NoInputs);
				ImGui::SliderFloat("Luminous power", &s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity.a, 0.f, 10.f);
			}
			break;
			default: SEAssertF("Invalid type");
			}

			static std::array<char, 64> s_nameInputBuffer = { "Spawned\0" };
			ImGui::InputText("Name", s_nameInputBuffer.data(), s_nameInputBuffer.size());

			if (ImGui::Button("Spawn"))
			{
				entt::entity sceneNode = fr::SceneNode::Create(*fr::EntityManager::Get(), s_nameInputBuffer.data(), entt::null);

				switch (static_cast<EntityToSpawn>(s_entitySelectionIdx))
				{
				case EntityToSpawn::AmbientLight:
				{
					re::Texture const* newIBL = fr::SceneManager::GetSceneData()->TryLoadUniqueTexture(
						s_spawnParams->m_ambientLightSpawnParams.m_filepath,
						re::Texture::ColorSpace::Linear).get();

					if (newIBL)
					{
						entt::entity newAmbientLight = fr::LightComponent::CreateDeferredAmbientLightConcept(
							*fr::EntityManager::Get(),
							newIBL);

						fr::EntityManager::Get()->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(newAmbientLight);
					}
				}
				break;
				case EntityToSpawn::DirectionalLight:
				{
					fr::LightComponent::AttachDeferredDirectionalLightConcept(
						*fr::EntityManager::Get(),
						sceneNode,
						std::format("{}_DirectionalLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity,
						s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
				}
				break;
				case EntityToSpawn::PointLight:
				{
					fr::LightComponent::AttachDeferredPointLightConcept(
						*fr::EntityManager::Get(),
						sceneNode,
						std::format("{}_PointLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity,
						s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
				}
				break;
				default: SEAssertF("Invalid type");
				}
			}

			ImGui::Unindent();
		}

		ImGui::End();
	}
}


