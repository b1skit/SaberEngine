#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Mesh.h"



namespace gr
{
	class Camera;
	class Light;
	class RenderMesh;
}

namespace fr
{
	class GameObject;


	class SceneData
	{
	public:
		SceneData(std::string const& sceneName);
		~SceneData() { Destroy(); }

		SceneData() = delete;
		SceneData(SceneData const&) = delete;
		SceneData(SceneData&&) = delete;
		SceneData& operator=(SceneData const&) = delete;

		void Destroy();

		bool Load(std::string const& relativeFilePath); // File name and path, relative to the ..\Scenes\ dir

		// SceneData metadata:
		inline std::string const& GetName() const { return m_name; }

		// Cameras:
		void AddCamera(std::shared_ptr<gr::Camera> newCamera);
		inline std::shared_ptr<gr::Camera> GetMainCamera()	const { return m_cameras.at(0); } // First camera added
		inline std::vector<std::shared_ptr<gr::Camera>> const& GetCameras() const { return m_cameras; }
		
		// Lights:
		void AddLight(std::shared_ptr<gr::Light> newLight);
		inline std::shared_ptr<gr::Light> GetAmbientLight() { return m_ambientLight; }
		inline std::shared_ptr<gr::Light> const GetAmbientLight() const { return m_ambientLight; }
		inline std::shared_ptr<gr::Light> GetKeyLight() { return m_keyLight; }
		inline std::shared_ptr<gr::Light> GetKeyLight() const { return m_keyLight; }
		inline std::vector<std::shared_ptr<gr::Light>>& GetPointLights() { return m_pointLights; }
		inline std::vector<std::shared_ptr<gr::Light>> const& GetPointLights() const { return m_pointLights; }

		// GameObjects:
		void AddGameObject(std::shared_ptr<fr::GameObject> newGameObject);
		inline std::vector<std::shared_ptr<fr::GameObject>> const& GetGameObjects() const { return m_gameObjects; }




		// TODO: these functions should become private once GLTF is implemented


		// RenderMeshes: (Currently internally populated when GameObjects are added)
		void AddRenderMesh(std::shared_ptr<gr::RenderMesh> newRenderMesh);
		inline std::vector<std::shared_ptr<gr::RenderMesh>>& GetRenderMeshes() { return m_renderMeshes; }
		inline std::vector<std::shared_ptr<gr::RenderMesh>> const& GetRenderMeshes() const { return m_renderMeshes; }
		// TODO: RenderMeshes are not currently directly used. Ideally, we should add GameObjects, which should update
		// the rendermeshes list, which should update the meshes list.
		// RenderMeshes and meshes should never need to be directly accessed beyond the SceneData; Their accessors
		// should be made private, and be used for convenience when generating render batches

		// Meshes: (Currently directly populated during load)
		void UpdateSceneBounds(std::shared_ptr<gr::Mesh> newMesh);
		inline std::vector<std::shared_ptr<gr::Mesh>> const& GetMeshes() const { return m_meshes; }
	
		// ^^^^^^^TODO: these functions should become private once GLTF is implemented



		// Textures:
		void AddUniqueTexture(std::shared_ptr<gr::Texture>& newTexture); // Updates texture pointer
		inline std::unordered_map<std::string, std::shared_ptr<gr::Texture>> const& GetTextures() const { return m_textures; }
		/*std::shared_ptr<gr::Texture> const GetTexture(std::string const& texPath) const;
		inline bool TextureExists(std::string const& texPath) const { return m_textures.find(texPath) != m_textures.end(); }*/

		// Gets already-loaded textures, or loads if it's unseen. Returns nullptr if texture file doesn't exist
		std::shared_ptr<gr::Texture> GetLoadTextureByPath(std::vector<std::string> texturePaths, bool returnErrorTex = false);


		// Materials:
		void AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial);
		inline std::unordered_map<std::string, std::shared_ptr<gr::Material>> const& GetMaterials() const { return m_materials; }
		std::shared_ptr<gr::Material> const GetMaterial(std::string const& materialName) const;
		inline bool MaterialExists(std::string const& matName) const { return m_materials.find(matName) != m_materials.end(); }

		// SceneData bounds:
		inline gr::Bounds const& GetWorldSpaceSceneBounds() const { return m_sceneWorldSpaceBounds; }


		// TODO: Objects should be identified via hashed names, instead of strings

	private:
		const std::string m_name;

		std::vector<std::shared_ptr<fr::GameObject>> m_gameObjects;
		std::vector<std::shared_ptr<gr::RenderMesh>> m_renderMeshes;
		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;

		std::unordered_map<std::string, std::shared_ptr<gr::Texture>> m_textures;
		std::unordered_map<std::string, std::shared_ptr<gr::Material>> m_materials;

		std::shared_ptr<gr::Light> m_ambientLight;
		std::shared_ptr<gr::Light> m_keyLight;
		std::vector<std::shared_ptr<gr::Light>> m_pointLights; 

		std::vector<std::shared_ptr<gr::Camera>> m_cameras;

		gr::Bounds m_sceneWorldSpaceBounds;
	};
}


