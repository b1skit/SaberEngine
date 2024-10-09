// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "Core/Util/HashUtils.h"


namespace gr
{
	class Material;
	class MeshPrimitive;
}

namespace re
{
	class Shader;
	class VertexStream;
}

namespace re
{
	class SceneData final
	{
	public:
		explicit SceneData();
		SceneData(SceneData&&) noexcept = default;
		SceneData& operator=(SceneData&&) noexcept = default;
		
		~SceneData();
		void Destroy();


	public:		
		re::Texture const* GetIBLTexture() const;

		// Geometry:
		bool AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>&); // Returns true if incoming ptr is modified
		bool AddUniqueVertexStream(std::shared_ptr<gr::VertexStream>&); // Returns true if incoming ptr is modified

		// Textures:
		bool AddUniqueTexture(std::shared_ptr<re::Texture>& newTexture); // Returns true if incoming ptr is modified
		std::shared_ptr<re::Texture> GetTexture(std::string const& texName) const;
		std::shared_ptr<re::Texture> const* GetTexturePtr(std::string const& texName) const;
		std::shared_ptr<re::Texture> TryGetTexture(std::string const& texName) const;
		bool TextureExists(std::string const& textureName) const;
		std::shared_ptr<re::Texture> TryLoadUniqueTexture(std::string const& filepath, re::Texture::ColorSpace);

		// Materials:
		void AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial);
		std::shared_ptr<gr::Material> GetMaterial(std::string const& materialName) const;
		bool MaterialExists(std::string const& matName) const;
		std::vector<std::string> GetAllMaterialNames() const;

		// Shaders:
		bool AddUniqueShader(std::shared_ptr<re::Shader>& newShader); // Returns true if new object was added
		std::shared_ptr<re::Shader> GetShader(ShaderID) const;
		bool ShaderExists(ShaderID) const;

		void EndLoading();

		
	private:
		std::unordered_map<DataHash, std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;
		mutable std::mutex m_meshPrimitivesMutex;

		std::unordered_map<DataHash, std::shared_ptr<gr::VertexStream>> m_vertexStreams;
		std::mutex m_vertexStreamsMutex;

		std::unordered_map<util::StringHash, std::shared_ptr<re::Texture>> m_textures;
		mutable std::shared_mutex m_texturesReadWriteMutex; // mutable, as we need to be able to modify it in const functions

		std::unordered_map<util::StringHash, std::shared_ptr<gr::Material>> m_materials;
		mutable std::shared_mutex m_materialsReadWriteMutex;

		std::unordered_map<size_t, std::shared_ptr<re::Shader>> m_shaders;
		mutable std::shared_mutex m_shadersReadWriteMutex;

		bool m_isCreated; // Validate Destroy() was called after a scene was loaded


	private:
		SceneData(SceneData const&) = delete;
		SceneData& operator=(SceneData const&) = delete;
	};


	inline void SceneData::EndLoading()
	{
		m_isCreated = true;
	}
}