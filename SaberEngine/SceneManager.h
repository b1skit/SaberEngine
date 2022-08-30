#pragma once

#include <vector>
#include <unordered_map>

#include <assimp/scene.h>		// Output data structure
#include <glm/glm.hpp>

#include "EngineComponent.h"
#include "EventListener.h"
#include "Mesh.h"
#include "Texture.h"


namespace gr
{
	class Material;
	class Mesh;
	class Texture;
	struct TextureParams;
	enum class TextureColorSpace;
	class Camera;
	class Light;
	class Transform;
}

namespace SaberEngine
{
	class aiTexture;
	class SceneObject;
	class GameObject;
	class Renderable;
	class Skybox;
	struct Bounds;
	struct Scene;
	enum CAMERA_TYPE;
}

using std::vector;
using std::unordered_map;
using glm::vec4;
using gr::Camera;


namespace SaberEngine
{
	// Scene Manager: Manages scenes
	class SceneManager : public virtual EngineComponent, public virtual EventListener
	{
	public:
		SceneManager(); // Reserve vector memory

		// Singleton functionality:
		static SceneManager& Instance();
		SceneManager(SceneManager const&)	= delete; // Disallow copying of our Singleton
		void operator=(SceneManager const&) = delete;

		// EngineComponent interface:
		void Startup();
		void Shutdown();
		void Update();
		void Destroy() {}	// Do nothing, for now...

		// EventListener interface:
		void HandleEvent(std::shared_ptr<EventInfo const> eventInfo);

		// Member functions:
		//------------------
		// Load a scene.
		// sceneName = the root folder name within the ./Scenes/ directory. Must contain an .fbx file with the same name
		bool LoadScene(string sceneName);

		inline unsigned int	NumMaterials() { return (int)m_materials.size(); }
		unordered_map<string, std::shared_ptr<gr::Material>> const&	GetMaterials() const;
		std::shared_ptr<gr::Material> GetMaterial(string materialName);
		
		vector<std::shared_ptr<gr::Mesh>> const* GetRenderMeshes(std::shared_ptr<gr::Material> targetMaterial);	// Returns ALL meshs if targetMaterial == nullptr
		vector<std::shared_ptr<Renderable>>* GetRenderables();

		std::shared_ptr<gr::Light> const& GetAmbientLight();
		std::shared_ptr<gr::Light> GetKeyLight();
		
		std::vector<std::shared_ptr<gr::Camera>> const& GetCameras(CAMERA_TYPE cameraType);
		std::shared_ptr<gr::Camera>	GetMainCamera();
		void RegisterCamera(CAMERA_TYPE cameraType, std::shared_ptr<gr::Camera> newCamera);;

		void AddTexture(std::shared_ptr<gr::Texture>& newTexture); // If duplicate texture exists, it will be deleted and the newTexture pointer updated to the correct address
		
		// Find if a texture if it exists, or try & load it if it doesn't. Returns nullptr if file isn't/can't be loaded
		std::shared_ptr<gr::Texture> FindLoadTextureByPath(
			string texturePath, 
			gr::Texture::TextureColorSpace colorSpace, 
			bool loadIfNotFound = true);	

		vector<std::shared_ptr<gr::Light>> const& GetDeferredLights();

		std::shared_ptr<Skybox>	GetSkybox();

		string const& GetCurrentSceneName() const;

		

	protected:


	private:
		// Scene management:
		//------------------
		std::shared_ptr<Scene> m_currentScene = nullptr;

		// Add a game object and register it with the various tracking lists
		void AddGameObject(std::shared_ptr<GameObject> newGameObject);

		// Material management:
		//---------------------
		unordered_map<string, std::shared_ptr<gr::Material>> m_materials;	// Hash table of scene Material pointers

		unordered_map<string, std::shared_ptr<gr::Texture>> m_textures;	// Hash table of scene Texture pointers

		void				AddMaterial(std::shared_ptr<gr::Material>& newMaterial);	// Add a material to the material array. Note: Material name MUST be unique

		void				AssembleMaterialMeshLists();		// Helper function: Compiles vectors filled with meshes that use each material. Must be called once after all meshes have finished loading
		unordered_map<string, vector<std::shared_ptr<gr::Mesh>>>m_materialMeshLists;	// Hash table: Maps material names, to a vector of std::shared_ptr<Mesh> using the material


		// Scene setup/construction:
		//--------------------------

		// Assimp scene material and texture import helper:
		void			ImportMaterialsAndTexturesFromScene(aiScene const* scene, string sceneName);

		// Import and configure scene skybox:
		void			ImportSky(string sceneName);
		
		// Assimp scene texture import helper:
		std::shared_ptr<gr::Texture> ExtractLoadTextureFromAiMaterial(aiTextureType textureType, aiMaterial* material, string sceneName);
		std::shared_ptr<gr::Texture> FindTextureByNameInAiMaterial(string nameSubstring, aiMaterial* material, string sceneName);

		// Assimp scene material property helper:
		bool			ExtractPropertyFromAiMaterial(aiMaterial* material, vec4& targetProperty, char const* AI_MATKEY_TYPE, int unused0 = 0, int unused1 = 0); // NOTE: unused0/unused1 are required to match #defined macros


		// Assimp scene geo import helper:
		void			ImportGameObjectGeometryFromScene(aiScene const* scene);

		// Scene geometry import helper: Create a GameObject transform hierarchy and return the GameObject parent. 
		// Note: Adds the GameObject to the currentScene's gameObjects
		std::shared_ptr<GameObject>		FindCreateGameObjectParents(aiScene const* scene, aiNode* parent);

		// Scene geometry import helper : Combines seperated transform nodes found throughout the scene graph.
		// Finds and combines the FIRST instance of Translation, Scaling, Rotation matrices in the parenting hierarchy
		aiMatrix4x4		GetCombinedTransformFromHierarchy(aiScene const* scene, aiNode* parent, bool skipPostRotations = true);
		void			InitializeTransformValues(aiMatrix4x4 const& source, gr::Transform* dest);	// Helper function: Copy transformation values from Assimp scene to SaberEngine transform

		// Light import helper: Initializes a SaberEngine Light's transform from an assimp scene. Calls InitializeTransformValues()
		void			InitializeLightTransformValues(aiScene const* scene, string lightName, gr::Transform* targetLightTransform);
		
		// Find a node with a name matching or containing name
		aiNode*			FindNodeContainingName(aiScene const* scene, string name);
		aiNode*			FindNodeRecursiveHelper(aiNode* rootNode, string name);	// Recursive helper function: Finds nodes containing name as a substring


		// Import light data from loaded scene
		void			ImportLightsFromScene(aiScene const* scene);

		// Import camera data from loaded scene
		void			ImportCamerasFromScene(aiScene const* scene = nullptr, bool clearCameras = false);
	};
}

