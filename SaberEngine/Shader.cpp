#include "Shader.h"
#include "Shader_Platform.h"
#include "DebugConfiguration.h"

using std::string;
using std::vector;
using std::shared_ptr;


namespace re
{
	class Sampler;
	class Texture;
}

namespace gr
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
		SetUniform(uniformName, texture.get(), gr::Shader::UniformType::Texture, 1);
		SetUniform(uniformName, sampler.get(), gr::Shader::UniformType::Sampler, 1);
	}
}
