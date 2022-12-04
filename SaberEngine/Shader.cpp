#include "Shader.h"
#include "Shader_Platform.h"
#include "DebugConfiguration.h"

using std::string;
using std::vector;
using std::shared_ptr;


namespace gr
{
	class Sampler;
	class Texture;


	Shader::Shader(string const& extensionlessShaderFilename) :
		NamedObject(extensionlessShaderFilename)
	{
		platform::Shader::CreatePlatformParams(*this);
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}


	void Shader::SetUniform(string const& uniformName, void* value, UniformType const type, int count)
	{
		platform::Shader::SetUniform(*this, uniformName, value, type, count);
	}
	

	void Shader::SetTextureSamplerUniform(
		string const& uniformName,
		shared_ptr<gr::Texture> texture,
		shared_ptr<gr::Sampler> sampler)
	{
		SetUniform(uniformName, texture.get(), gr::Shader::UniformType::Texture, 1);
		SetUniform(uniformName, sampler.get(), gr::Shader::UniformType::Sampler, 1);
	}


	void Shader::SetParameterBlock(re::ParameterBlock const& paramBlock)
	{
		platform::Shader::SetParameterBlock(*this, paramBlock);
	}
}
