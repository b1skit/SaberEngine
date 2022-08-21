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


namespace SaberEngine
{
	// Predeclarations:
	class Light;
	class Camera;
	class Skybox;
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
		Scene(string sceneName);
		~Scene();

		// Meshes:
		//--------
		// Allocate an empty mesh array. Clears any existing mesh array
		void	InitMeshArray();

		int		AddMesh(std::shared_ptr<gr::Mesh> newMesh);
		void	DeleteMeshes();
		std::shared_ptr<gr::Mesh>	GetMesh(int meshIndex);
		inline vector<std::shared_ptr<gr::Mesh>> const& GetMeshes() { return m_meshes; }

		// Cameras:
		//---------
		vector<std::shared_ptr<Camera>> const&	GetCameras(CAMERA_TYPE cameraType);
		std::shared_ptr<Camera>					GetMainCamera()		{ return m_sceneCameras[CAMERA_TYPE_MAIN].at(0); }
		void					RegisterCamera(CAMERA_TYPE cameraType, std::shared_ptr<Camera> newCamera);
		void					ClearCameras();	// Destroys the scene's cameras

		void					AddLight(std::shared_ptr<Light> newLight);

		// Scene object containers:
		//-------------------------
		vector<GameObject*> m_gameObjects;	// Pointers to dynamically allocated GameObjects
		vector<Renderable*> m_renderables;	// Pointers to statically allocated renderables held by GameObjects


		// Duplicate pointers to lights contained in deferredLights
		std::shared_ptr<Light> m_ambientLight = nullptr;
		std::shared_ptr<Light> m_keyLight		= nullptr;

		vector<std::shared_ptr<Light>> const& GetDeferredLights() const		{ return m_deferredLights; }

		// Skybox object:
		std::shared_ptr<Skybox> m_skybox		= nullptr;

		inline gr::Bounds const& WorldSpaceSceneBounds() const	{ return m_sceneWorldBounds; }

		string const& GetSceneName() const { return m_sceneName; }

	private:
		vector<vector<std::shared_ptr<Camera>>> m_sceneCameras;

		vector<std::shared_ptr<gr::Mesh>> m_meshes;				// Pointers to dynamically allocated Mesh objects

		gr::Bounds m_sceneWorldBounds;

		// Lights:
		//--------
		vector<std::shared_ptr<Light>> m_deferredLights;

		string m_sceneName;
	};
}


