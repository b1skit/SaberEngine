// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Mesh.h"
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
	class SceneNodeEntity;


	class SceneData final : public virtual en::NamedObject
	{
	public:
		typedef uint64_t DataHash;

	public:
		explicit SceneData(std::string const& sceneName);
		SceneData(SceneData&&) = default;
		SceneData& operator=(SceneData&&) = default;
		
		~SceneData();
		void Destroy();

		bool Load(std::string const& relativeFilePath); // Filename and path, relative to the ..\Scenes\ dir
		void PostLoadFinalize(); // Executes post-load callbacks

	public:
		// Cameras:
		std::vector<std::shared_ptr<gr::Camera>> const& GetCameras() const;
		std::shared_ptr<gr::Camera> GetMainCamera(uint64_t uniqueID) const;

	public:		
		// Lights:
		void AddLight(std::shared_ptr<gr::Light> newLight);
		std::shared_ptr<gr::Light> const GetAmbientLight() const;
		std::shared_ptr<gr::Light> GetKeyLight() const;
		std::vector<std::shared_ptr<gr::Light>> const& GetPointLights() const;

		std::shared_ptr<re::Texture> GetIBLTexture() const;

		// Meshes:
		void AddMesh(std::shared_ptr<gr::Mesh> mesh);
		std::vector <std::shared_ptr<gr::Mesh>> const& GetMeshes() const;
		bool AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>&); // Returns true if incoming ptr is modified

		bool AddUniqueVertexStream(std::shared_ptr<re::VertexStream>&); // Returns true if incoming ptr is modified

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
		void UpdateSceneBounds();

		// Post loading finalization callback: Allow objects that require the scene to be fully loaded to complete their
		// initialization
		void RegisterForPostLoadCallback(std::function<void()>);


		// Interfaces that self-register/self-remove themselves:
	protected:
		friend class gr::Camera; // Only Camera objects can register/unregister themselves
		void AddCamera(std::shared_ptr<gr::Camera> newCamera); // Returns the camera index
		void RemoveCamera(uint64_t uniqueID);


	private:
		void UpdateSceneBounds(std::shared_ptr<gr::Mesh> mesh);


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_meshes;
		std::unordered_map<DataHash, std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;
		mutable std::mutex m_meshesAndMeshPrimitivesMutex;

		std::unordered_map<DataHash, std::shared_ptr<re::VertexStream>> m_vertexStreams;
		std::mutex m_vertexStreamsMutex;

		std::unordered_map<size_t, std::shared_ptr<re::Texture>> m_textures;
		mutable std::shared_mutex m_texturesReadWriteMutex; // mutable, as we need to be able to modify it in const functions

		std::unordered_map<size_t, std::shared_ptr<gr::Material>> m_materials;
		mutable std::shared_mutex m_materialsReadWriteMutex;

		std::unordered_map<size_t, std::shared_ptr<re::Shader>> m_shaders;
		mutable std::shared_mutex m_shadersReadWriteMutex;

		std::shared_ptr<gr::Light> m_ambientLight;
		std::shared_mutex m_ambientLightReadWriteMutex;

		std::shared_ptr<gr::Light> m_keyLight;
		std::shared_mutex m_keyLightReadWriteMutex;

		std::vector<std::shared_ptr<gr::Light>> m_pointLights;
		std::shared_mutex m_pointLightsReadWriteMutex;

		std::vector<std::shared_ptr<gr::Camera>> m_cameras;
		mutable std::shared_mutex m_camerasReadWriteMutex;

		gr::Bounds m_sceneWorldSpaceBounds;
		mutable std::mutex m_sceneBoundsMutex;

		std::vector<std::function<void()>> m_postLoadCallbacks;
		std::mutex m_postLoadCallbacksMutex;

		bool m_finishedLoading; // Used to assert scene data is not accessed while it might potentially be modified


	private:
		SceneData() = delete;
		SceneData(SceneData const&) = delete;
		SceneData& operator=(SceneData const&) = delete;

		// Friends:
		friend class opengl::SceneData;
	};
}


