#include "Shader.h"
#include "Shader_Platform.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"

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


	void Shader::SetUniform(char const* uniformName, void const* value, platform::Shader::UNIFORM_TYPE const& type, int count /*= 1*/)
	{
		platform::Shader::SetUniform(*this, uniformName, value, type, count);
	}


	void Shader::Create()
	{
		platform::Shader::Create(*this);
	}


	void Shader::Bind(bool doBind)
	{
		platform::Shader::Bind(*this, doBind);
	}

	void Shader::SetTexture(
		string const& shaderName,
		shared_ptr<gr::Texture> texture,
		shared_ptr<gr::Sampler const> sampler) const
	{
		platform::Shader::SetTexture(*this, shaderName, texture, sampler);
	}
}
