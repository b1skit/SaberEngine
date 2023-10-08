// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CastUtils.h"
#include "Config.h"
#include "Light.h"
#include "Mesh.h"
#include "ParameterBlock.h"
#include "PerformanceTimer.h"
#include "SceneManager.h"
#include "Transform.h"

using fr::SceneData;
using en::Config;
using re::Batch;
using re::ParameterBlock;
using gr::Transform;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::mat4;


namespace
{
	constexpr size_t k_initialBatchReservations = 100;
}

namespace en
{
	SceneManager* SceneManager::Get()
	{
		static std::unique_ptr<en::SceneManager> instance = std::make_unique<en::SceneManager>();
		return instance.get();
	}


	SceneManager::SceneManager() 
		: m_sceneData(nullptr)
		, m_activeCameraIdx(0)
	{
		m_sceneBatches.reserve(k_initialBatchReservations);
	}


	void SceneManager::Startup()
	{
		LOG("SceneManager starting...");

		PerformanceTimer timer;
		timer.Start();

		// Load the scene:
		string sceneName;
		if (Config::Get()->TryGetValue<string>(en::ConfigKeys::k_sceneNameValueName, sceneName))
		{
			m_sceneData = std::make_shared<SceneData>(sceneName);
		}
		else
		{
			LOG_WARNING("No scene name found to load");			
			m_sceneData = std::make_shared<SceneData>("Empty Scene");
		}

		string sceneFilePath;
		Config::Get()->TryGetValue<string>(en::ConfigKeys::k_sceneFilePathValueName, sceneFilePath);

		const bool loadResult = m_sceneData->Load(sceneFilePath);
		if (!loadResult)
		{
			LOG_ERROR("Failed to load scene: %s", sceneFilePath);
			EventManager::Get()->Notify(EventManager::EventInfo{ EventManager::EngineQuit });
		}

		m_sceneData->PostLoadFinalize();

		LOG("\nSceneManager::Startup complete in %f seconds...\n", timer.StopSec());
	}


	void SceneManager::Shutdown()
	{
		LOG("Scene manager shutting down...");

		m_sceneData = nullptr;
	}


	void SceneManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Tick Updateables:
		for (int i = 0; i < (int)m_sceneData->GetUpdateables().size(); i++)
		{
			m_sceneData->GetUpdateables().at(i)->Update(stepTimeMs);
		}

		// Recompute Scene Bounds. This also recomputes all Transforms in a DFS ordering
		m_sceneData->RecomputeSceneBounds();
	}


	void SceneManager::FinalUpdate()
	{
		BuildSceneBatches();
	}


	void SceneManager::SetMainCameraIdx(size_t camIdx)
	{
		EventManager::Get()->Notify(EventManager::EventInfo{ EventManager::CameraSelectionChanged });

		m_activeCameraIdx = camIdx;
	}


	std::shared_ptr<gr::Camera> SceneManager::GetMainCamera() const
	{
		// TODO: This camera is accessed multiple times before the first frame is rendered (e.g. PlayerObject, various
		// graphics systems). Currently, this is fine as we currently join any loading threads before creating these
		// objects, but it may not always be the case.

		return m_sceneData->GetCameras()[m_activeCameraIdx];
	}


	void SceneManager::ShowImGuiWindow(bool* show)
	{
		constexpr char const* scenePanelWindowTitle = "Scene Objects";
		ImGui::Begin(scenePanelWindowTitle, show);
	
		
		if (ImGui::CollapsingHeader("Cameras:", ImGuiTreeNodeFlags_None))
		{
			std::vector<std::shared_ptr<gr::Camera>> const& cameras = m_sceneData->GetCameras();

			// TODO: Currently, we set the camera parameters as a permanent PB via a shared_ptr from the main camera
			// once in every GS. We need to be able to get/set the main camera's camera params PB every frame, in every
			// GS. Camera selection works, but the GS's all render from the same camera. For now, just disable it.
//#define CAMERA_SELECTION
#if defined(CAMERA_SELECTION)
			static int activeCamIdx = static_cast<int>(m_activeCameraIdx);
			constexpr ImGuiComboFlags k_cameraSelectionflags = 0;
			static int comboSelectedCamIdx = activeCamIdx; // Initialize with the index of the current main camera
			const char* comboPreviewCamName = cameras[comboSelectedCamIdx]->GetName().c_str();
			if (ImGui::BeginCombo("Active camera", comboPreviewCamName, k_cameraSelectionflags))
			{
				for (size_t camIdx = 0; camIdx < cameras.size(); camIdx++)
				{
					const bool isSelected = (comboSelectedCamIdx == camIdx);
					if (ImGui::Selectable(cameras[camIdx]->GetName().c_str(), isSelected))
					{
						comboSelectedCamIdx = static_cast<int>(camIdx);
					}

					if (isSelected) // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();

				// Handle active camera changes:
				if (comboSelectedCamIdx != activeCamIdx)
				{
					activeCamIdx = comboSelectedCamIdx;
					SetMainCameraIdx(comboSelectedCamIdx);
				}
			}
#endif
			
			for (size_t camIdx = 0; camIdx < cameras.size(); camIdx++)
			{
				cameras[camIdx]->ShowImGuiWindow();
				ImGui::Separator();
			}
		}
		
		if (ImGui::CollapsingHeader("Meshes:", ImGuiTreeNodeFlags_None))
		{
			std::vector<std::shared_ptr<gr::Mesh>> const& meshes = m_sceneData->GetMeshes();
			for (auto const& mesh : meshes)
			{
				mesh->ShowImGuiWindow();
				ImGui::Separator();
			}
		}

		if (ImGui::CollapsingHeader("Materials:", ImGuiTreeNodeFlags_None))
		{
			std::unordered_map<size_t, std::shared_ptr<gr::Material>> const& materials = m_sceneData->GetMaterials();
			for (auto const& materialEntry : materials)
			{
				materialEntry.second->ShowImGuiWindow();
				ImGui::Separator();
			}
		}

		if (ImGui::CollapsingHeader("Lights:", ImGuiTreeNodeFlags_None))
		{
			std::shared_ptr<gr::Light> const ambientLight = m_sceneData->GetAmbientLight();
			if (ambientLight)
			{
				ambientLight->ShowImGuiWindow();
				ImGui::Separator();
			}

			std::shared_ptr<gr::Light> const directionalLight = m_sceneData->GetKeyLight();
			if (directionalLight)
			{
				directionalLight->ShowImGuiWindow();
				ImGui::Separator();
			}

			if (ImGui::CollapsingHeader("Point Lights:", ImGuiTreeNodeFlags_None))
			{
				std::vector<std::shared_ptr<gr::Light>> const& pointLights = m_sceneData->GetPointLights();
				for (auto const& light : pointLights)
				{
					light->ShowImGuiWindow();
				}
				ImGui::Separator();
			}
		}

		ImGui::End();
	}


	std::vector<re::Batch>& SceneManager::GetSceneBatches()
	{
		// NOTE: The caller should std::move this; m_sceneBatches must be empty for the next BuildSceneBatches call
		return m_sceneBatches;
	};


	void SceneManager::BuildSceneBatches()
	{
		SEAssert("Scene batches should be empty", m_sceneBatches.empty());

		std::vector<shared_ptr<gr::Mesh>> const& sceneMeshes = SceneManager::GetSceneData()->GetMeshes();
		if (sceneMeshes.empty())
		{
			return;
		}

		m_sceneBatches = std::move(re::Batch::BuildBatches(SceneManager::GetSceneData()->GetMeshes()));
	}
}


