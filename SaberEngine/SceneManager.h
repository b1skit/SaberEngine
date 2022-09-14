#pragma once

#include <vector>
#include <unordered_map>

#include <assimp/scene.h>		// Output data structure
#include <glm/glm.hpp>

#include "EngineComponent.h"
#include "EventListener.h"
#include "Mesh.h"
#include "Texture.h"
#include "Scene.h"


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

namespace fr
{
	class GameObject;
}


namespace en
{
	// SceneData Manager: Manages scenes
	class SceneManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		SceneManager();
		~SceneManager() = default;

		SceneManager(SceneManager const&) = delete;
		SceneManager(SceneManager&&) = delete;
		void operator=(SceneManager const&) = delete;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override;

		inline std::shared_ptr<fr::SceneData> GetScene() { return m_currentScene; }
		inline std::shared_ptr<fr::SceneData const> const GetScene() const { return m_currentScene; }



		// Member functions:
		//------------------
		// Load a scene.
		// sceneName = the root folder name within the ./Scenes/ directory. Must contain an .fbx file with the same name
		bool LoadScene(std::string const& sceneName);

		
		
		



	private:

		std::shared_ptr<fr::SceneData> m_currentScene = nullptr;



		

		// TODO: Transition to GLTF2.0, moving all scene loading logic to SceneData
		
		// Assimp scene material and texture import helper:
		void ImportMaterialsAndTexturesFromScene(aiScene const* scene, std::string const& sceneName);
		
		// Assimp scene texture import helper:
		std::shared_ptr<gr::Texture> ExtractLoadTextureFromAiMaterial(
			aiTextureType textureType, aiMaterial* material, std::string const& sceneName);
		std::shared_ptr<gr::Texture> FindTextureByNameInAiMaterial(
			std::string nameSubstring, aiMaterial* material, std::string const& sceneName);

		// Assimp scene material property helper:
		bool ExtractPropertyFromAiMaterial(aiMaterial* material, glm::vec4& targetProperty, char const* AI_MATKEY_TYPE, int unused0 = 0, int unused1 = 0); // NOTE: unused0/unused1 are required to match #defined macros

		// Assimp scene geo import helper:
		void ImportGameObjectGeometryFromScene(aiScene const* scene);

		// SceneData geometry import helper: Create a GameObject transform hierarchy and return the GameObject parent. 
		// Note: Adds the GameObject to the currentScene's gameObjects
		std::shared_ptr<fr::GameObject>	FindCreateGameObjectParents(aiScene const* scene, aiNode* parent);

		// SceneData geometry import helper : Combines seperated transform nodes found throughout the scene graph.
		// Finds and combines the FIRST instance of Translation, Scaling, Rotation matrices in the parenting hierarchy
		aiMatrix4x4	GetCombinedTransformFromHierarchy(aiScene const* scene, aiNode* parent, bool skipPostRotations = true);
		void InitializeTransformValues(aiMatrix4x4 const& source, gr::Transform* dest);	// Helper function: Copy transformation values from Assimp scene to SaberEngine transform

		// Light import helper: Initializes a SaberEngine Light's transform from an assimp scene. Calls InitializeTransformValues()
		void InitializeLightTransformValues(aiScene const* scene, std::string const& lightName, gr::Transform* targetLightTransform);
		
		// Find a node with a name matching or containing name
		aiNode*	FindNodeContainingName(aiScene const* scene, std::string name);
		aiNode*	FindNodeRecursiveHelper(aiNode* rootNode, std::string const& name);	// Recursive helper function: Finds nodes containing name as a substring

		// Import light data from loaded scene
		void ImportLightsFromScene(aiScene const* scene);

		// Import camera data from loaded scene
		void ImportCamerasFromScene(aiScene const* scene = nullptr, bool clearCameras = false);
	};
}

