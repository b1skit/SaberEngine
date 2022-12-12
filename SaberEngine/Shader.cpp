#include "Shader.h"
#include "Shader_Platform.h"
#include "DebugConfiguration.h"
#include "Material.h"

using std::string;
using std::vector;
using std::shared_ptr;
using gr::Material;

namespace re
{
	Shader::Shader(string const& extensionlessShaderFilename) :
		NamedObject(extensionlessShaderFilename)
	{
		platform::Shader::CreatePlatformParams(*this);

		LoadShaderTexts();
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}


	void Shader::LoadShaderTexts()
	{
		m_shaderTexts.clear();
		platform::Shader::LoadShaderTexts(GetName(), m_shaderTexts);
		SEAssert("Failed to load any shader text", !m_shaderTexts.empty());
	}


	void Shader::SetUniform(string const& uniformName, void* value, UniformType const type, int count)
	{
		platform::Shader::SetUniform(*this, uniformName, value, type, count);
	}
	

	void Shader::SetTextureSamplerUniform(
		string const& uniformName,
		shared_ptr<re::Texture> texture,
		shared_ptr<re::Sampler> sampler)
	{
		SetUniform(uniformName, texture.get(), Shader::UniformType::Texture, 1);
		SetUniform(uniformName, sampler.get(), Shader::UniformType::Sampler, 1);
	}


	void Shader::SetMaterial(gr::Material* material)
	{
		SEAssert("Cannot bind incomplete material", 
			material->GetTexureSlotDescs().size() > 0 && material->GetParameterBlock() != nullptr);

		for (size_t i = 0; i < material->GetTexureSlotDescs().size(); i++)
		{
			if (material->GetTexureSlotDescs()[i].m_texture)
			{
				SetTextureSamplerUniform(
					material->GetTexureSlotDescs()[i].m_shaderSamplerName,
					material->GetTexureSlotDescs()[i].m_texture,
					material->GetTexureSlotDescs()[i].m_samplerObject);
			}
		}

		platform::Shader::SetParameterBlock(*this, *material->GetParameterBlock().get());
	}
}
