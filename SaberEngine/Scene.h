#pragma once

#include "Mesh.h"

#include <string>
#include <vector>

using std::string;
using std::vector;


// Initial allocation amounts
#define GAMEOBJECTS_RESERVATION_AMT				100		// TODO: Set these with more carefully selected values...
#define RENDERABLES_RESERVATION_AMT				100
#define MESHES_RESERVATION_AMT					100

#define DEFERRED_LIGHTS_RESERVATION_AMT			25

#define CAMERA_TYPE_SHADOW_ARRAY_SIZE			10
#define CAMERA_TYPE_REFLECTION_ARRAY_SIZE		10

namespace gr
{
	class Camera;
	class Light;
}

namespace SaberEngine
{
	class GameObject;
	class Renderable;
	

	// Indexes for scene cameras used for different rendering roles
	// Note: Cameras are rendered in the order defined here
	enum CAMERA_TYPE 
	{
		CAMERA_TYPE_SHADOW,
		CAMERA_TYPE_REFLECTION,
		CAMERA_TYPE_MAIN,			// The primary scene camera

		CAMERA_TYPE_COUNT			// Reserved: The number of camera types
	};


	// Container for all scene data:
	struct Scene
	{
		Scene(std::string sceneName);
		~Scene();

		// Meshes:
		//--------
		// Allocate an empty mesh array. Clears any existing mesh array
		void InitMeshArray();

		int	AddMesh(std::shared_ptr<gr::Mesh> newMesh);
		void DeleteMeshes();
		std::shared_ptr<gr::Mesh> GetMesh(int meshIndex);
		inline std::vector<std::shared_ptr<gr::Mesh>> const& GetMeshes() { return m_meshes; }

		// Cameras:
		//---------
		std::vector<std::shared_ptr<gr::Camera>> const& GetCameras(CAMERA_TYPE cameraType);
		std::shared_ptr<gr::Camera> GetMainCamera()	{ return m_sceneCameras[CAMERA_TYPE_MAIN].at(0); }
		void RegisterCamera(CAMERA_TYPE cameraType, std::shared_ptr<gr::Camera> newCamera);
		void ClearCameras();

		void AddLight(std::shared_ptr<gr::Light> newLight);

		// Scene object containers:
		//-------------------------
		std::vector<std::shared_ptr<GameObject>> m_gameObjects;
		std::vector<std::shared_ptr<Renderable>> m_renderables;	// Pointers to Renderables held by GameObjects


		// Pointers to point lights also contained in m_deferredLights
		std::shared_ptr<gr::Light> m_ambientLight = nullptr;
		std::shared_ptr<gr::Light> m_keyLight = nullptr;
		std::vector<std::shared_ptr<gr::Light>> m_pointLights; 

		std::vector<std::shared_ptr<gr::Light>> const& GetDeferredLights() const	{ return m_deferredLights; }

		inline gr::Bounds const& WorldSpaceSceneBounds() const { return m_sceneWorldBounds; }

		std::string const& GetSceneName() const { return m_sceneName; }

	private:
		std::vector<std::vector<std::shared_ptr<gr::Camera>>> m_sceneCameras;

		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;	// Pointers to dynamically allocated Mesh objects

		gr::Bounds m_sceneWorldBounds;

		std::vector<std::shared_ptr<gr::Light>> m_deferredLights; // Pointers to all lights of all types
		
		std::string m_sceneName;

		// TODO: Move initialization to ctor init list
	};
}


