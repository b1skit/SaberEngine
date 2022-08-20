#pragma once

#include <vector>
#include <unordered_map>

#include <assimp/scene.h>		// Output data structure
#include <glm/glm.hpp>

#include "EngineComponent.h"
#include "EventListener.h"
#include "grMesh.h"
#include "grTexture.h"




using std::vector;
using std::unordered_map;

using glm::vec4;


// Pre-declarations:
namespace gr
{
	class Mesh;
	class Texture;
	struct TextureParams;
	enum class TextureColorSpace;
	
}
namespace SaberEngine
{
	class Camera;
	class aiTexture;
	class SceneObject;
	class GameObject;
	class Material;
	class Renderable;
	class Light;
	class Transform;
	class Skybox;
	struct Bounds;
	struct Scene;
	enum CAMERA_TYPE;
}


namespace SaberEngine
{
	// Scene Manager: Manages scenes
	class SceneManager : public virtual EngineComponent, public EventListener
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
		void HandleEvent(EventInfo const* eventInfo);

		// Member functions:
		//------------------
		// Load a scene.
		// sceneName = the root folder name within the ./Scenes/ directory. Must contain an .fbx file with the same name
		bool LoadScene(string sceneName);

		inline unsigned int						NumMaterials()								{ return (int)m_materials.size(); }
		unordered_map<string, Material*> const&	GetMaterials() const;
		Material*								GetMaterial(string materialName);
		
		vector<gr::Mesh*> const* GetRenderMeshes(Material* targetMaterial);	// Returns ALL meshs if targetMaterial == nullptr
		vector<Renderable*>*					GetRenderables();

		Light* const&							GetAmbientLight();
		Light*									GetKeyLight();
		
		vector<Camera*> const&					GetCameras(CAMERA_TYPE cameraType);
		Camera*									GetMainCamera();
		void									RegisterCamera(CAMERA_TYPE cameraType, Camera* newCamera);;

		void AddTexture(std::shared_ptr<gr::Texture>& newTexture); // If duplicate texture exists, it will be deleted and the newTexture pointer updated to the correct address
		
		// Find if a texture if it exists, or try & load it if it doesn't. Returns nullptr if file isn't/can't be loaded
		std::shared_ptr<gr::Texture> FindLoadTextureByPath(
			string texturePath, 
			gr::Texture::TextureColorSpace colorSpace, 
			bool loadIfNotFound = true);	

		vector<Light*> const&					GetDeferredLights();

		Skybox*									GetSkybox();

		string									GetCurrentSceneName() const;

		

	protected:


	private:
		// Scene management:
		//------------------
		Scene* m_currentScene = nullptr;

		// Add a game object and register it with the various tracking lists
		void AddGameObject(GameObject* newGameObject);

		// Material management:
		//---------------------
		unordered_map<string, Material*> m_materials;	// Hash table of scene Material pointers

		unordered_map<string, std::shared_ptr<gr::Texture>> m_textures;	// Hash table of scene Texture pointers

		void				AddMaterial(Material*& newMaterial);	// Add a material to the material array. Note: Material name MUST be unique

		void				AssembleMaterialMeshLists();		// Helper function: Compiles vectors filled with meshes that use each material. Must be called once after all meshes have finished loading
		unordered_map<string, vector<gr::Mesh*>>m_materialMeshLists;	// Hash table: Maps material names, to a vector of Mesh* using the material


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
		GameObject*		FindCreateGameObjectParents(aiScene const* scene, aiNode* parent);

		// Scene geometry import helper : Combines seperated transform nodes found throughout the scene graph.
		// Finds and combines the FIRST instance of Translation, Scaling, Rotation matrices in the parenting hierarchy
		aiMatrix4x4		GetCombinedTransformFromHierarchy(aiScene const* scene, aiNode* parent, bool skipPostRotations = true);
		void			InitializeTransformValues(aiMatrix4x4 const& source, Transform* dest);	// Helper function: Copy transformation values from Assimp scene to SaberEngine transform

		// Light import helper: Initializes a SaberEngine Light's transform from an assimp scene. Calls InitializeTransformValues()
		void			InitializeLightTransformValues(aiScene const* scene, string lightName, Transform* targetLightTransform);
		
		// Find a node with a name matching or containing name
		aiNode*			FindNodeContainingName(aiScene const* scene, string name);
		aiNode*			FindNodeRecursiveHelper(aiNode* rootNode, string name);	// Recursive helper function: Finds nodes containing name as a substring


		// Import light data from loaded scene
		void			ImportLightsFromScene(aiScene const* scene);

		// Import camera data from loaded scene
		void			ImportCamerasFromScene(aiScene const* scene = nullptr, bool clearCameras = false);
	};
}

