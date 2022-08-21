#include "Scene.h"
#include "Light.h"
#include "Camera.h"
#include "Skybox.h"
#include "GameObject.h"
#include "Renderable.h"
#include "BuildConfiguration.h"


namespace SaberEngine
{
	Scene::Scene(string sceneName)
	{
		m_sceneName = sceneName;

		m_gameObjects.reserve(GAMEOBJECTS_RESERVATION_AMT);
		m_renderables.reserve(RENDERABLES_RESERVATION_AMT);
		m_meshes.reserve(MESHES_RESERVATION_AMT);

		m_sceneCameras.reserve(CAMERA_TYPE_COUNT);
		for (int i = 0; i < CAMERA_TYPE_COUNT; i++)
		{
			m_sceneCameras.push_back(vector<Camera*>());
		}
		m_sceneCameras.at(CAMERA_TYPE_SHADOW).reserve(CAMERA_TYPE_SHADOW_ARRAY_SIZE);
		m_sceneCameras.at(CAMERA_TYPE_REFLECTION).reserve(CAMERA_TYPE_REFLECTION_ARRAY_SIZE);

		m_sceneCameras.at(CAMERA_TYPE_MAIN).reserve(1); // Only 1 main camera

		m_deferredLights.reserve(DEFERRED_LIGHTS_RESERVATION_AMT);
	}


	Scene::~Scene()
	{
		DeleteMeshes();

		for (int i = 0; i < (int)m_gameObjects.size(); i++)
		{
			if (m_gameObjects.at(i))
			{
				delete m_gameObjects.at(i);
				m_gameObjects.at(i) = nullptr;
			}
		}


		for (int i = 0; i < (int)m_deferredLights.size(); i++)
		{
			m_deferredLights.at(i) = nullptr;
		}
		m_deferredLights.clear();

		ClearCameras();

		m_skybox = nullptr;
	}


	void Scene::InitMeshArray()
	{
		DeleteMeshes();
		m_meshes.reserve(MESHES_RESERVATION_AMT);
	}


	int Scene::AddMesh(std::shared_ptr<gr::Mesh> newMesh)
	{
		// Update scene (world) bounds to contain the new mesh:
		gr::Bounds meshWorldBounds(newMesh->GetLocalBounds().GetTransformedBounds(newMesh->GetTransform().Model()));

		if (meshWorldBounds.xMin() < m_sceneWorldBounds.xMin())
		{
			m_sceneWorldBounds.xMin() = meshWorldBounds.xMin();
		}
		if (meshWorldBounds.xMax() > m_sceneWorldBounds.xMax())
		{
			m_sceneWorldBounds.xMax() = meshWorldBounds.xMax();
		}

		if (meshWorldBounds.yMin() < m_sceneWorldBounds.yMin())
		{
			m_sceneWorldBounds.yMin() = meshWorldBounds.yMin();
		}
		if (meshWorldBounds.yMax() > m_sceneWorldBounds.yMax())
		{
			m_sceneWorldBounds.yMax() = meshWorldBounds.yMax();
		}

		if (meshWorldBounds.zMin() < m_sceneWorldBounds.zMin())
		{
			m_sceneWorldBounds.zMin() = meshWorldBounds.zMin();
		}
		if (meshWorldBounds.zMax() > m_sceneWorldBounds.zMax())
		{
			m_sceneWorldBounds.zMax() = meshWorldBounds.zMax();
		}

		// Add the mesh to our array:
		int meshIndex = (int)m_meshes.size();
		m_meshes.push_back(newMesh);
		return meshIndex;

	}


	void Scene::DeleteMeshes()
	{
		for (int i = 0; i < (int)m_meshes.size(); i++)
		{
			m_meshes.at(i) = nullptr;
		}
		m_meshes.clear();
	}


	std::shared_ptr<gr::Mesh> Scene::GetMesh(int meshIndex)
	{
		if (meshIndex >= (int)m_meshes.size())
		{
			LOG_ERROR("Invalid mesh index received: " + std::to_string(meshIndex) + " > " + std::to_string((int)m_meshes.size()) + ". Returning nullptr");
			return nullptr;
		}

		return m_meshes.at(meshIndex);
	}


	void Scene::RegisterCamera(CAMERA_TYPE cameraType, Camera* newCamera)
	{
		if (newCamera != nullptr && (int)cameraType < (int)m_sceneCameras.size())
		{
			m_sceneCameras.at((int)cameraType).push_back(newCamera);

			LOG("Registered new camera \"" + newCamera->GetName() + "\"");
		}
		else
		{
			LOG_ERROR("Failed to register new camera!");
		}
	}


	vector<Camera*> const& Scene::GetCameras(CAMERA_TYPE cameraType)
	{
		return m_sceneCameras.at(cameraType);
	}


	void Scene::ClearCameras()
	{
		if (m_sceneCameras.empty())
		{
			return;
		}

		for (int i = 0; i < (int)m_sceneCameras.size(); i++)
		{
			for (int j = 0; j < (int)m_sceneCameras.at(i).size(); j++)
			{
				if (m_sceneCameras.at(i).at(j) != nullptr)
				{
					m_sceneCameras.at(i).at(j)->Destroy();
					delete m_sceneCameras.at(i).at(j);
					m_sceneCameras.at(i).at(j) = nullptr;
				}
			}
		}
	}

	void Scene::AddLight(std::shared_ptr<Light> newLight)
	{
		switch (newLight->Type())
		{
			// Check if we've got any existing ambient or directional lights:
		case LIGHT_AMBIENT_COLOR:
		case LIGHT_AMBIENT_IBL:
		case LIGHT_DIRECTIONAL:
		{
			bool foundExisting = false;
			for (int currentLight = 0; currentLight < (int)m_deferredLights.size(); currentLight++)
			{
				if (m_deferredLights.at(currentLight)->Type() == newLight->Type())
				{
					foundExisting = true;
					LOG_ERROR("Found an existing light with type " + std::to_string((int)newLight->Type()) + ". New light will not be added");
					break;
				}
			}
			if (!foundExisting)
			{
				m_deferredLights.push_back(newLight);

				if (newLight->Type() == LIGHT_DIRECTIONAL)
				{
					m_keyLight = newLight;
				}
				else
				{
					m_ambientLight = newLight;
				}
			}

			break;
		}

		// Don't need to do anything special with other light types
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_AREA:
		case LIGHT_TUBE:
		default:
			m_deferredLights.push_back(newLight);
			break;
		}
	}	
}