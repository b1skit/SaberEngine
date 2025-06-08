// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Private/Renderer/Shader.h"


namespace droid
{
	droid::ErrorCode BuildShaderFile_GLSL(
		std::vector<std::string> const& shaderSrcDirs,
		std::string const& extensionlessSrcFilename,
		uint64_t variantID,
		std::string const& entryPointName,
		re::Shader::ShaderType,
		std::vector<std::string> const& defines,
		std::string const& outputDir);
}