// © 2022 Adam Badke. All rights reserved.
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
}
