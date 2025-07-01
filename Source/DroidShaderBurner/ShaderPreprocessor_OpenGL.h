// � 2024 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/Shader.h"


namespace droid
{
	void BuildShaderFile_GLSL(
		std::vector<std::string> const& shaderSrcDirs,
		std::string const& extensionlessSrcFilename,
		uint64_t variantID,
		std::string const& entryPointName,
		re::Shader::ShaderType,
		std::vector<std::string> const& defines,
		std::string const& outputDir);
}