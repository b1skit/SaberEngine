#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "Mesh.h"
#include "Updateable.h"
#include "NamedObject.h"


namespace gr
{
	class Camera;
	class Light;
}

namespace fr
{
	class SceneObject;


	class SceneData : public virtual en::NamedObject
	{
	public:
		explicit SceneData(std::string const& sceneName);
		~SceneData() { Destroy(); }

		void Destroy();

		bool Load(std::string const& relativeFilePath); // Filename and path, relative to the ..\Scenes\ dir

		// Cameras:
		void AddCamera(std::shared_ptr<gr::Camera> newCamera);
	
		inline std::shared_ptr<gr::Camera> GetMainCamera() const { return m_cameras.at(0); } // First camera added
		inline std::vector<std::shared_ptr<gr::Camera>> const& GetCameras() const { return m_cameras; }
		
		// Lights:
		void AddLight(std::shared_ptr<gr::Light> newLight);
		inline std::shared_ptr<gr::Light> GetAmbientLight() { return m_ambientLight; }
		inline std::shared_ptr<gr::Light> const GetAmbientLight() const { return m_ambientLight; }
		inline std::shared_ptr<gr::Light> GetKeyLight() { return m_keyLight; }
		inline std::shared_ptr<gr::Light> GetKeyLight() const { return m_keyLight; }
		inline std::vector<std::shared_ptr<gr::Light>>& GetPointLights() { return m_pointLights; }
		inline std::vector<std::shared_ptr<gr::Light>> const& GetPointLights() const { return m_pointLights; }

		// SceneObjects:
		void AddSceneObject(std::shared_ptr<fr::SceneObject> sceneObject); // Internally adds Updateable & Mesh

		void AddUpdateable(std::shared_ptr<en::Updateable> updateable);
		inline std::vector<std::shared_ptr<en::Updateable>> const& GetUpdateables() const { return m_updateables; }

		// Meshes:
		inline std::vector<std::shared_ptr<re::MeshPrimitive>> const& GetMeshPrimitives() const { return m_meshPrimitives; }

		// Textures:
		void AddUniqueTexture(std::shared_ptr<gr::Texture>& newTexture); // Note: newTexture may be modified

		// Gets already-loaded textures, or loads if it's unseen. Returns nullptr if texture file doesn't exist
		std::shared_ptr<gr::Texture> GetLoadTextureByPath(std::vector<std::string> texturePaths, bool returnErrorTex = false);

		// Materials:
		void AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial);
		std::shared_ptr<gr::Material const> GetMaterial(std::string const& materialName) const;
		inline bool MaterialExists(std::string const& matName) const;

		// SceneData bounds:
		inline re::Bounds const& GetWorldSpaceSceneBounds() const { return m_sceneWorldSpaceBounds; }

	private:
		void AddMesh(std::shared_ptr<gr::Mesh> mesh);
		void UpdateSceneBounds(std::shared_ptr<re::MeshPrimitive> meshPrimitive);

	private:
		std::vector<std::shared_ptr<en::Updateable>> m_updateables;
		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_meshPrimitives;

		std::unordered_map<size_t, std::shared_ptr<gr::Texture>> m_textures;
		std::unordered_map<size_t, std::shared_ptr<gr::Material>> m_materials;

		std::shared_ptr<gr::Light> m_ambientLight;
		std::shared_ptr<gr::Light> m_keyLight;
		std::vector<std::shared_ptr<gr::Light>> m_pointLights; 

		std::vector<std::shared_ptr<gr::Camera>> m_cameras;

		re::Bounds m_sceneWorldSpaceBounds;


	private:
		SceneData() = delete;
		SceneData(SceneData const&) = delete;
		SceneData(SceneData&&) = delete;
		SceneData& operator=(SceneData const&) = delete;
	};
}


