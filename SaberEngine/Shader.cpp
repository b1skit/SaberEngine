#include "Shader.h"
#include "Shader_Platform.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

using std::string;
using std::vector;
using std::shared_ptr;
using gr::Material;


namespace gr
{
	class Sampler;
	class Texture;


	Shader::Shader(string const& shaderName) :
		m_shaderName{shaderName}
	{
		platform::Shader::PlatformParams::CreatePlatformParams(*this);
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}


	void Shader::SetUniform(string const& uniformName, void const* value, platform::Shader::UniformType const type, int count) const
	{
		platform::Shader::SetUniform(*this, uniformName, value, type, count);
	}


	void Shader::Create()
	{
		platform::Shader::Create(*this);
	}


	void Shader::Bind(bool doBind) const
	{
		platform::Shader::Bind(*this, doBind);
	}
	

	void Shader::SetTextureSamplerUniform(
		string const& uniformName,
		shared_ptr<gr::Texture> texture,
		shared_ptr<gr::Sampler const> sampler) const
	{
		SetUniform(uniformName, texture.get(), platform::Shader::UniformType::Texture, 1);
		SetUniform(uniformName, sampler.get(), platform::Shader::UniformType::Sampler, 1);
	}


	void Shader::SetParameterBlock(re::PermanentParameterBlock const& paramBlock) const
	{
		platform::Shader::SetParameterBlock(*this, paramBlock);
	}
}
