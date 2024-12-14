// © 2022 Adam Badke. All rights reserved.
#include "AssetLoadUtils.h"
#include "MeshPrimitive.h"
#include "SceneData.h"
#include "Sampler.h"
#include "Shader.h"

#include "Core/Assert.h"
#include "Core/Config.h"

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
		CreateSamplerLibrary();
	}


	void SceneData::Destroy()
	{
		{
			std::scoped_lock lock(
				m_meshPrimitivesMutex,
				m_vertexStreamsMutex,
				m_texturesReadWriteMutex,
				m_materialsReadWriteMutex,
				m_shadersReadWriteMutex,
				m_samplersReadWriteMutex);

			m_meshPrimitives.clear();
			m_vertexStreams.clear();
			m_textures.clear();
			m_materials.clear();
			m_shaders.clear();
			m_samplers.clear();
		}

		m_isCreated = false; // Flag that Destroy has been called
	}


	re::Texture const* SceneData::GetIBLTexture() const
	{
		// We search for a scene-specific IBL, and fallback to the engine default IBL if it's not found
		std::shared_ptr<re::Texture> iblTexture = nullptr;
		std::string sceneIBLPath;
		bool result = core::Config::Get()->TryGetValue<std::string>(core::configkeys::k_sceneIBLPathKey, sceneIBLPath);
		if (result)
		{
			iblTexture = TryGetTexture(sceneIBLPath);
		}
		
		if (!iblTexture)
		{
			std::string const& defaultIBLPath = 
				core::Config::Get()->GetValueAsString(core::configkeys::k_defaultEngineIBLPathKey);
			iblTexture = GetTexture(defaultIBLPath); // Guaranteed to exist
		}

		return iblTexture.get();
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


	bool SceneData::AddUniqueTexture(std::shared_ptr<re::Texture>& newTexture)
	{
		SEAssert(newTexture != nullptr, "Cannot add null texture to textures table");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_texturesReadWriteMutex);

			const auto texturePosition = m_textures.find(newTexture->GetNameHash());

			bool foundExisting = false;
			if (texturePosition != m_textures.end()) // Found existing
			{
				LOG("Texture \"%s\" has alredy been registed with scene", newTexture->GetName().c_str());
				newTexture = texturePosition->second;
				foundExisting = true;
			}
			else  // Add new
			{
				m_textures[newTexture->GetNameHash()] = newTexture;
				LOG("Texture \"%s\" registered with scene", newTexture->GetName().c_str());
			}

			return foundExisting;
		}
	}


	std::shared_ptr<re::Texture> SceneData::GetTexture(std::string const& texName) const
	{
		const util::StringHash nameHash = util::StringHash(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);

			auto result = m_textures.find(nameHash);

			SEAssert(result != m_textures.end(), "Texture with that name does not exist");

			return result->second;
		}
	}


	std::shared_ptr<re::Texture> const* SceneData::GetTexturePtr(std::string const& texName) const
	{
		const util::StringHash nameHash(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);

			SEAssert(m_textures.contains(nameHash), "Texture with that name does not exist");

			return &m_textures.at(nameHash);
		}
	}


	std::shared_ptr<re::Texture> SceneData::TryGetTexture(std::string const& texName) const
	{
		const util::StringHash nameHash(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);

			auto result = m_textures.find(nameHash);
			return result == m_textures.end() ? nullptr : result->second;
		}
	}


	bool SceneData::TextureExists(std::string const& texName) const
	{
		const util::StringHash nameHash(texName);
		{
			std::shared_lock<std::shared_mutex> readLock(m_texturesReadWriteMutex);
			return m_textures.find(nameHash) != m_textures.end();
		}
	}


	std::shared_ptr<re::Texture> SceneData::TryLoadUniqueTexture(
		std::string const& filepath, re::Texture::ColorSpace colorSpace)
	{
		std::shared_ptr<re::Texture> result = TryGetTexture(filepath);

		SEAssert(result == nullptr || result->GetTextureParams().m_colorSpace == colorSpace,
			"Found a texture with the same filepath name, but different colorspace. This is unexpected");

		if (result == nullptr)
		{
			result = grutil::LoadTextureFromFilePath({ filepath }, colorSpace, false);
			if (result)
			{
				AddUniqueTexture(result);
			}
		}

		return result;
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


	bool SceneData::AddUniqueSampler(std::shared_ptr<re::Sampler>& newSampler)
	{
		{
			std::unique_lock<std::shared_mutex> samplerLock(m_samplersReadWriteMutex);

			bool addedNewSampler = false;

			auto samplerItr = m_samplers.find(newSampler->GetNameHash());
			if (samplerItr != m_samplers.end()) // Found existing
			{
				newSampler = samplerItr->second;
			}
			else
			{
				m_samplers.emplace(newSampler->GetNameHash(), newSampler);
				addedNewSampler = true;
				LOG("Sampler \"%s\" registered with the scene", newSampler->GetName().c_str());
			}

			return addedNewSampler;
		}
	}


	std::shared_ptr<re::Sampler> SceneData::GetSampler(util::StringHash const& samplerName)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_samplersReadWriteMutex);

			SEAssert(m_samplers.contains(samplerName), "Invalid sampler name");
			return m_samplers.at(samplerName);
		}
	}


	bool SceneData::SamplerExists(util::StringHash const& samplerName) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_samplersReadWriteMutex);
			return m_samplers.contains(samplerName);
		}
	}


	void SceneData::CreateSamplerLibrary()
	{
		// Internally, our Samplers will self-register with the SceneData. So we just trigger that here

		constexpr re::Sampler::SamplerDesc k_wrapMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapMinMagLinearMipPoint", k_wrapMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagLinearMipPoint", k_clampMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagMipPoint", k_clampMinMagMipPoint);

		constexpr re::Sampler::SamplerDesc k_borderMinMagMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Border,
			.m_edgeModeV = re::Sampler::EdgeMode::Border,
			.m_edgeModeW = re::Sampler::EdgeMode::Border,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WhiteBorderMinMagMipPoint", k_borderMinMagMipPoint);

		constexpr re::Sampler::SamplerDesc k_clampMinMagMipLinear = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
			.m_edgeModeU = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeV = re::Sampler::EdgeMode::Clamp,
			.m_edgeModeW = re::Sampler::EdgeMode::Clamp,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("ClampMinMagMipLinear", k_clampMinMagMipLinear);

		constexpr re::Sampler::SamplerDesc k_wrapMinMagMipLinear = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::MIN_MAG_MIP_LINEAR,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapMinMagMipLinear", k_wrapMinMagMipLinear);

		constexpr re::Sampler::SamplerDesc k_wrapAnisotropic = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::ANISOTROPIC,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::None,
			.m_borderColor = re::Sampler::BorderColor::TransparentBlack,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapAnisotropic", k_wrapAnisotropic);

		// PCF Samplers
		constexpr float k_maxLinearDepth = std::numeric_limits<float>::max();

		constexpr re::Sampler::SamplerDesc k_borderCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Border,
			.m_edgeModeV = re::Sampler::EdgeMode::Border,
			.m_edgeModeW = re::Sampler::EdgeMode::Border,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("BorderCmpMinMagLinearMipPoint", k_borderCmpMinMagLinearMipPoint);

		constexpr re::Sampler::SamplerDesc k_wrapCmpMinMagLinearMipPoint = re::Sampler::SamplerDesc
		{
			.m_filterMode = re::Sampler::FilterMode::COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			.m_edgeModeU = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeV = re::Sampler::EdgeMode::Wrap,
			.m_edgeModeW = re::Sampler::EdgeMode::Wrap,
			.m_mipLODBias = 0.f,
			.m_maxAnisotropy = 16,
			.m_comparisonFunc = re::Sampler::ComparisonFunc::Less,
			.m_borderColor = re::Sampler::BorderColor::OpaqueWhite,
			.m_minLOD = 0,
			.m_maxLOD = std::numeric_limits<float>::max() // No limit
		};
		std::ignore = re::Sampler::Create("WrapCmpMinMagLinearMipPoint", k_wrapCmpMinMagLinearMipPoint);
	}
}