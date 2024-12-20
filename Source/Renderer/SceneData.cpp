// © 2022 Adam Badke. All rights reserved.
#include "AssetLoadUtils.h"
#include "MeshPrimitive.h"
#include "SceneData.h"
#include "Sampler.h"
#include "Shader.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/Inventory.h"

#include "Core/Interfaces/INamedObject.h"


namespace re
{
	SceneData::SceneData()
		: m_isCreated(false)
	{
	}


	SceneData::~SceneData()
	{
		SEAssert(m_isCreated == false, "Did the SceneData go out of scope before Destroy was called?");
	}


	void SceneData::Initialize()
	{
		//
	}


	void SceneData::Destroy()
	{
		{
			std::scoped_lock lock(
				m_meshPrimitivesMutex,
				m_vertexStreamsMutex,
				m_materialsReadWriteMutex,
				m_shadersReadWriteMutex);

			m_meshPrimitives.clear();
			m_vertexStreams.clear();
			m_materials.clear();
			m_shaders.clear();
		}

		m_isCreated = false; // Flag that Destroy has been called
	}


	bool SceneData::AddUniqueMeshPrimitive(std::shared_ptr<gr::MeshPrimitive>& meshPrimitive)
	{
		const uint64_t meshPrimitiveDataHash = meshPrimitive->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_meshPrimitivesMutex);

			auto const& result = m_meshPrimitives.find(meshPrimitiveDataHash);
			if (result != m_meshPrimitives.end())
			{
				LOG("MeshPrimitive \"%s\" has the same data hash as an existing MeshPrimitive. It will be replaced "
					"with a shared copy",
					meshPrimitive->GetName().c_str());

				meshPrimitive = result->second;
				replacedIncomingPtr = true;

				//// Add a marker to simplify debugging of shared meshes
				//constexpr char const* k_sharedMeshTag = " <shared>";
				//if (meshPrimitive->GetName().find(k_sharedMeshTag) == std::string::npos)
				//{
				//	meshPrimitive->SetName(meshPrimitive->GetName() + k_sharedMeshTag);
				//}
				
				// BUG HERE: We (currently) can't set the name on something that is shared, as another thread might be
				// using it (e.g. dereferencing .c_str())
			}
			else
			{
				m_meshPrimitives.insert({ meshPrimitiveDataHash, meshPrimitive });
			}
		}
		return replacedIncomingPtr;
	}


	bool SceneData::AddUniqueVertexStream(std::shared_ptr<gr::VertexStream>& vertexStream)
	{
		const uint64_t vertexStreamDataHash = vertexStream->GetDataHash();
		bool replacedIncomingPtr = false;
		{
			std::lock_guard<std::mutex> lock(m_vertexStreamsMutex);

			auto const& result = m_vertexStreams.find(vertexStreamDataHash);
			if (result != m_vertexStreams.end())
			{
				LOG(std::format("Vertex stream has the same data hash \"{}\" as an existing vertex stream. It will be "
					"replaced with a shared copy", vertexStreamDataHash).c_str());

				vertexStream = result->second;
				replacedIncomingPtr = true;
			}
			else
			{
				m_vertexStreams.insert({ vertexStreamDataHash, vertexStream });
			}
		}
		return replacedIncomingPtr;
	}


	void SceneData::AddUniqueMaterial(std::shared_ptr<gr::Material>& newMaterial)
	{
		SEAssert(newMaterial != nullptr, "Cannot add null material to material table");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_materialsReadWriteMutex);

			// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
			const auto matPosition = m_materials.find(newMaterial->GetNameHash());
			if (matPosition != m_materials.end()) // Found existing
			{
				newMaterial = matPosition->second;
			}
			else // Add new
			{
				m_materials[newMaterial->GetNameHash()] = newMaterial;
				LOG("Material \"%s\" registered to scene data", newMaterial->GetName().c_str());
			}
		}
	}


	std::shared_ptr<gr::Material> SceneData::GetMaterial(std::string const& materialName) const
	{
		const util::StringHash nameHash(materialName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);
			const auto matPos = m_materials.find(nameHash);

			SEAssert(matPos != m_materials.end(), "Could not find material");

			return matPos->second;
		}
	}


	bool SceneData::MaterialExists(std::string const& matName) const
	{
		const util::StringHash nameHash(matName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);

			return m_materials.find(nameHash) != m_materials.end();
		}
	}


	std::vector<std::string> SceneData::GetAllMaterialNames() const
	{
		std::vector<std::string> result;
		{
			std::shared_lock<std::shared_mutex> readLock(m_materialsReadWriteMutex);

			for (auto const& material : m_materials)
			{
				result.emplace_back(material.second->GetName());
			}
		}
		return result;
	}


	bool SceneData::AddUniqueShader(std::shared_ptr<re::Shader>& newShader)
	{
		SEAssert(newShader != nullptr, "Cannot add null shader to shader table");
		{
			std::unique_lock<std::shared_mutex> writeLock(m_shadersReadWriteMutex);

			bool addedNewShader = false;

			const ShaderID shaderIdentifier = newShader->GetShaderIdentifier();

			auto shaderItr = m_shaders.find(shaderIdentifier);
			if (shaderItr != m_shaders.end()) // Found existing
			{
				newShader = shaderItr->second;
			}
			else // Add new
			{
				m_shaders[shaderIdentifier] = newShader;
				addedNewShader = true;
				LOG("Shader \"%s\" (ID %llu) registered with scene",
					newShader->GetName().c_str(),
					newShader->GetShaderIdentifier());
			}
			return addedNewShader;
		}
	}


	std::shared_ptr<re::Shader> SceneData::GetShader(ShaderID shaderID) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_shadersReadWriteMutex);
			std::unordered_map<size_t, std::shared_ptr<re::Shader>>::const_iterator shaderPos = m_shaders.find(shaderID);

			SEAssert(shaderPos != m_shaders.end(), "Could not find shader");

			return shaderPos->second;
		}
	}


	bool SceneData::ShaderExists(ShaderID shaderID) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_shadersReadWriteMutex);

			return m_shaders.find(shaderID) != m_shaders.end();
		}
	}
}