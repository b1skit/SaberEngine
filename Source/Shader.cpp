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
	}


	void Shader::Destroy()
	{
		platform::Shader::Destroy(*this);
	}
}
