#include "Material.h"
#include "CoreEngine.h"
#include "Shader.h"
#include "Texture.h"
#include "DebugConfiguration.h"
#include "Shader.h"
#include "CoreEngine.h"

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;
using std::unordered_map;
using gr::Shader;
using gr::Texture;
using gr::Sampler;
using SaberEngine::CoreEngine;

using std::make_unique;
using std::make_shared;
using std::vector;


namespace gr
{
	// Static members:
	unique_ptr<unordered_map<string, shared_ptr<Material::MaterialDefinition>>> Material::m_materialLibrary = nullptr;


	shared_ptr<Material::MaterialDefinition const> Material::GetMaterialDefinition(std::string const& matName)
	{
		if (Material::m_materialLibrary == nullptr)
		{
			m_materialLibrary = make_unique<unordered_map<string, shared_ptr<Material::MaterialDefinition>>>();

			// TODO: Materials should be described externally; for now, we hard code them

			// PBR materials, written to the gBuffer
			shared_ptr<MaterialDefinition> pbrMat = make_shared<MaterialDefinition>();
			pbrMat->m_definitionName = "PBRMaterial";
			pbrMat->m_textureSlots = 
			{
				{nullptr, Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear), "MatAlbedo" },
				{nullptr, Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear), "MatNormal" },
				{nullptr, Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear), "MatRMAO" },
				{nullptr, Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear), "MatEmissive" },
			};
			pbrMat->m_propertySlots =
			{
				{"MatProperty0", glm::vec4(0,0,0,0)}
			};
			pbrMat->m_shader = nullptr; // Don't need a shader; PBR materials are written directly to the GBuffer
			m_materialLibrary->insert({ pbrMat->m_definitionName, pbrMat });
		}

		auto result = Material::m_materialLibrary->find(matName);

		SEAssert("Invalid Material name", result != Material::m_materialLibrary->end());

		return result->second;
	}


	Material::Material(std::string const& name, std::shared_ptr<MaterialDefinition const> matDefinition) :
		m_name(name),
		m_texSlots(matDefinition->m_textureSlots),
		m_shader(matDefinition->m_shader),
		m_properties(matDefinition->m_propertySlots)
	{
		// Build a map from shader sampler name, to texture slot index:
		for (size_t i = 0; i < m_texSlots.size(); i++)
		{
			m_namesToSlotIndex.insert({ m_texSlots[i].m_shaderSamplerName, (uint32_t)i});
		}		
	}


	void Material::Destroy()
	{
		m_name += "_DESTROYED";
		m_shader = nullptr;
		m_texSlots.clear();
		m_properties.clear();
	}


	void Material::BindToShader(std::shared_ptr<gr::Shader const> shaderOverride)
	{
		shared_ptr<gr::Shader const> shader = shaderOverride == nullptr ? m_shader : shaderOverride;

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

		for (size_t i = 0; i < m_properties.size(); i++)
		{
			shader->SetUniform(
				m_properties[i].m_propertyName.c_str(), 
				&m_properties[i].m_property, 
				platform::Shader::UniformType::Vec4f, 
				1);
		}
	}


	std::shared_ptr<gr::Texture>& Material::GetTexture(std::string const& samplerName)
	{
		return const_cast<std::shared_ptr<gr::Texture>&>(const_cast<const Material*>(this)->GetTexture(samplerName));
	}


	std::shared_ptr<gr::Texture>const& Material::GetTexture(std::string const& samplerName) const
	{
		auto index = m_namesToSlotIndex.find(samplerName);

		SEAssert("Invalid sampler name",
			index != m_namesToSlotIndex.end() && 
			(uint32_t)index->second < (uint32_t)m_texSlots.size());

		return m_texSlots[index->second].m_texture;
	}
}

