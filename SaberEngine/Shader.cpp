#include <fstream>

#include <GL/glew.h> // TODO: DELETE THIS!!!!!!!!!!!!!!!!!!
#include "Shader_OpenGL.h" // TEMP HAX!!!!!!!!!!!!!!!!!

#include "Shader.h"
#include "Shader_Platform.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "Material.h"
#include "Texture.h"

using std::ifstream;
using std::string;
using std::vector;
using std::shared_ptr;
using gr::Material;
using gr::Texture;


namespace gr
{
	// Static members:
	const vector<string> Shader::k_ShaderKeywords
	{
		"NO_ALBEDO_TEXTURE",
		"NO_NORMAL_TEXTURE",
		"NO_EMISSIVE_TEXTURE",
		"NO_RMAO_TEXTURE",
		"NO_COSINE_POWER",
	};


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


	void Shader::Create(vector<string> const* shaderKeywords)
	{
		platform::Shader::Create(*this, shaderKeywords);
	}


	void Shader::Create()
	{
		platform::Shader::Create(*this, nullptr);
	}


	void Shader::Bind(bool doBind)
	{
		platform::Shader::Bind(*this, doBind);
	}
}
