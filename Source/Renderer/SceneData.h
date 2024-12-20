// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Texture.h"

#include "Core/InvPtr.h"

#include "Core/Util/HashUtils.h"
#include "Core/Util/StringHash.h"


namespace gr
{
	class Material;
	class MeshPrimitive;
	class VertexStream;
}

namespace re
{
	class Sampler;
	class Shader;
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

	public:
		void Initialize();
		void Destroy();

	public:		
		// Geometry:
		bool AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>&); // Returns true if incoming ptr is modified
		bool AddUniqueVertexStream(std::shared_ptr<gr::VertexStream>&); // Returns true if incoming ptr is modified

		// Materials:
		void AddUniqueMaterial(std::shared_ptr<gr::Material>&);
		std::shared_ptr<gr::Material> GetMaterial(std::string const& materialName) const;
		bool MaterialExists(std::string const& matName) const;
		std::vector<std::string> GetAllMaterialNames() const;

		// Shaders:
		bool AddUniqueShader(std::shared_ptr<re::Shader>&); // Returns true if new object was added
		std::shared_ptr<re::Shader> GetShader(ShaderID) const;
		bool ShaderExists(ShaderID) const;

		void EndLoading();

		
	private:
		std::unordered_map<util::DataHash, std::shared_ptr<gr::MeshPrimitive>> m_meshPrimitives;
		mutable std::mutex m_meshPrimitivesMutex;

		std::unordered_map<util::DataHash, std::shared_ptr<gr::VertexStream>> m_vertexStreams;
		std::mutex m_vertexStreamsMutex;

		std::unordered_map<util::StringHash, std::shared_ptr<gr::Material>> m_materials;
		mutable std::shared_mutex m_materialsReadWriteMutex;

		std::unordered_map<ShaderID, std::shared_ptr<re::Shader>> m_shaders;
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