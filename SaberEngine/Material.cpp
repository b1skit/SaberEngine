#include <mutex>

#include "Material.h"
#include "Shader_Platform.h"
#include "Texture.h"
#include "DebugConfiguration.h"
#include "Shader.h"
#include "ParameterBlock.h"

using re::Shader;
using re::Texture;
using re::Sampler;
using re::ParameterBlock;

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::unordered_map;
using std::make_unique;
using std::make_shared;
using std::vector;
using glm::vec4;
using glm::vec3;


namespace gr
{
	// Static members:
	unique_ptr<unordered_map<string, shared_ptr<Material::MaterialDefinition>>> Material::m_materialLibrary = nullptr;


	shared_ptr<Material::MaterialDefinition const> Material::GetMaterialDefinition(std::string const& matName)
	{
		static std::mutex matLibraryMutex;
		std::unique_lock<std::mutex> samplerLock(matLibraryMutex);

		if (Material::m_materialLibrary == nullptr)
		{
			// TODO: Materials should be described externally; for now, we hard code them
			m_materialLibrary = make_unique<unordered_map<string, shared_ptr<Material::MaterialDefinition>>>();

			// GLTF Metallic-Roughness PBR Material:
			// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
			//--------------------------------------
			shared_ptr<MaterialDefinition> gltfPBRMat = make_shared<MaterialDefinition>();
			gltfPBRMat->m_definitionName = "pbrMetallicRoughness";
			gltfPBRMat->m_textureSlots =
			{
				{nullptr, Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear), "MatAlbedo" },
				{nullptr, Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear), "MatMetallicRoughness" }, // G = roughness, B = metalness. R & A are unused.
				{nullptr, Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear), "MatNormal" },
				{nullptr, Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear), "MatOcclusion" },
				{nullptr, Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear), "MatEmissive" },
			};
			gltfPBRMat->m_shader = nullptr; // Don't need a shader; PBR materials are written directly to the GBuffer
			m_materialLibrary->insert({ gltfPBRMat->m_definitionName, gltfPBRMat });
		}

		auto const& result = Material::m_materialLibrary->find(matName);

		SEAssert("Invalid Material name", result != Material::m_materialLibrary->end());

		return result->second;
	}


	Material::Material(string const& name, shared_ptr<MaterialDefinition const> matDefinition) :
			NamedObject(name),
		m_texSlots(matDefinition->m_textureSlots),
		m_shader(matDefinition->m_shader),
		m_matParams(nullptr)
	{
		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i});
		}
	}


	void Material::Destroy()
	{
		m_shader = nullptr;
		m_texSlots.clear();
	}


	void Material::BindToShader(std::shared_ptr<re::Shader> shaderOverride)
	{
		shared_ptr<re::Shader> shader = shaderOverride == nullptr ? m_shader : shaderOverride;

		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			if (m_texSlots[i].m_texture != nullptr)
			{
				shader->SetTextureSamplerUniform(
					m_texSlots[i].m_shaderSamplerName, 
					m_texSlots[i].m_texture,
					m_texSlots[i].m_samplerObject);
			}
		}

		if (m_matParams)
		{
			platform::Shader::SetParameterBlock(*shader, *m_matParams.get());
		}
	}


	std::shared_ptr<re::Texture>& Material::GetTexture(std::string const& samplerName)
	{
		return const_cast<std::shared_ptr<re::Texture>&>(const_cast<const Material*>(this)->GetTexture(samplerName));
	}


	std::shared_ptr<re::Texture> const& Material::GetTexture(std::string const& samplerName) const
	{
		auto const& index = m_namesToSlotIndex.find(samplerName);

		SEAssert("Invalid sampler name",
			index != m_namesToSlotIndex.end() && 
			(uint32_t)index->second < (uint32_t)m_texSlots.size());

		return m_texSlots[index->second].m_texture;
	}
}

