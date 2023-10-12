// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Mesh.h"
#include "Updateable.h"
#include "Transformable.h"
#include "NamedObject.h"


namespace gr
{
	class Camera;
	class Light;
}

namespace opengl
{
	class SceneData;
}

namespace re
{
	class Shader;
}

namespace fr
{
	class SceneNode;


	class SceneData final : public virtual en::NamedObject
	{
	public:
		typedef uint64_t DataHash;

	public:
		explicit SceneData(std::string const& sceneName);
		SceneData(SceneData&&) = default;
		SceneData& operator=(SceneData&&) = default;
		~SceneData() { Destroy(); }

		void Destroy();

		bool Load(std::string const& relativeFilePath); // Filename and path, relative to the ..\Scenes\ dir
		void PostLoadFinalize();

		// Cameras:
	protected:
		friend class gr::Camera;
		size_t AddCamera(std::shared_ptr<gr::Camera> newCamera); // Returns the camera index
	public:
		
		std::vector<std::shared_ptr<gr::Camera>> const& GetCameras() const;
		

	public:		
		// Lights:
		void AddLight(std::shared_ptr<gr::Light> newLight);
		inline std::shared_ptr<gr::Light> const GetAmbientLight() const { return m_ambientLight; }
		inline std::shared_ptr<gr::Light> GetKeyLight() const { return m_keyLight; }
		inline std::vector<std::shared_ptr<gr::Light>> const& GetPointLights() const { return m_pointLights; }

		std::shared_ptr<re::Texture> GetIBLTexture() const;

		// Updateables:
		void AddUpdateable(std::shared_ptr<en::Updateable> updateable);
		std::vector<std::shared_ptr<en::Updateable>> const& GetUpdateables() const;

		// Transformation hierarchy:
		void AddSceneNode(std::shared_ptr<fr::SceneNode> transformable);

		// Meshes:
		void AddMesh(std::shared_ptr<gr::Mesh> mesh);
		std::vector <std::shared_ptr<gr::Mesh>> const& GetMeshes() const;
		bool AddUniqueMeshPrimitive(std::shared_ptr<re::MeshPrimitive>&); // Returns true if incoming ptr is modified

		// Textures:
		bool AddUniqueTexture(std::shared_ptr<re::Texture>& newTexture); // Returns true if incoming ptr is modified
		std::shared_ptr<re::Texture> GetTexture(std::string const& texName) const;
		std::shared_ptr<re::Texture> TryGetTexture(std::string const& texName) const;
		bool TextureExists(std::string const& textureName) const;

		// Materials:
		void AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial);
		std::shared_ptr<gr::Material> GetMaterial(std::string const& materialName) const;
		std::unordered_map<size_t, std::shared_ptr<gr::Material>> const& GetMaterials() const;
		bool MaterialExists(std::string const& matName) const;

		bool AddUniqueShader(std::shared_ptr<re::Shader>& newShader); // Returns true if new object was added
		std::shared_ptr<re::Shader> GetShader(uint64_t shaderIdentifier) const;
		bool ShaderExists(uint64_t shaderIdentifier) const;

		// SceneData bounds:
		gr::Bounds const& GetWorldSpaceSceneBounds() const;
		void RecomputeSceneBounds();

	private:
		void UpdateSceneBounds(std::shared_ptr<gr::Mesh> mesh);

	private:
		std::vector<std::shared_ptr<en::Updateable>> m_updateables;
		std::mutex m_updateablesMutex;

		std::vector<std::shared_ptr<fr::SceneNode>> m_sceneNodes; // Transformation hierarchy
		std::mutex m_sceneNodesMutex;

		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;
		std::unordered_map<DataHash, std::shared_ptr<re::MeshPrimitive>> m_meshPrimitives;
		std::mutex m_meshesAndMeshPrimitivesMutex;		

		std::unordered_map<size_t, std::shared_ptr<re::Texture>> m_textures;
		mutable std::shared_mutex m_texturesMutex; // mutable, as we need to be able to modify it in const functions

		std::unordered_map<size_t, std::shared_ptr<gr::Material>> m_materials;
		mutable std::shared_mutex m_materialsMutex;

		std::unordered_map<size_t, std::shared_ptr<re::Shader>> m_shaders;
		mutable std::shared_mutex m_shadersMutex;

		std::shared_ptr<gr::Light> m_ambientLight;
		std::mutex m_ambientLightMutex;

		std::shared_ptr<gr::Light> m_keyLight;
		std::mutex m_keyLightMutex;

		std::vector<std::shared_ptr<gr::Light>> m_pointLights;
		std::mutex m_pointLightsMutex;

		std::vector<std::shared_ptr<gr::Camera>> m_cameras;
		std::mutex m_camerasMutex;

		gr::Bounds m_sceneWorldSpaceBounds;
		std::mutex m_sceneBoundsMutex;
		

		bool m_finishedLoading; // Used to assert scene data is not accessed while it might potentially be modified


	private:
		SceneData() = delete;
		SceneData(SceneData const&) = delete;
		SceneData& operator=(SceneData const&) = delete;

		// Friends:
		friend class opengl::SceneData;
	};
}


