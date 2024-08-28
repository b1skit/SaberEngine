// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/Shader.h"


namespace droid
{
	droid::ErrorCode BuildShaderFile(
		std::vector<std::string> const& shaderSrcDirs,
		std::string const& extensionlessSrcFilename,
		re::Shader::ShaderType,
		std::string const& outputDir);
}