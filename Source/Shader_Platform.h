// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Shader.h"


namespace re
{
	class ParameterBlock;
	class Sampler;
	class Texture;
	class Shader;
}

namespace platform
{
	class Shader
	{
	public:
		static void CreatePlatformParams(re::Shader& shader);

		// TODO: These helpers probably belong in the OpenGL namespace
		static std::string LoadShaderText(const std::string& filepath); // Loads file "filepath" within the shaders dir

		static void	InsertIncludedFiles(std::string& shaderText);
		static void	InsertDefines(std::string& shaderText, std::vector<std::string> const* shaderKeywords);
		

		// Static pointers:
		static void (*Create)(re::Shader& shader);
		static void (*Bind)(re::Shader&);
		static void (*SetUniform)(
			re::Shader& shader, 
			std::string const& uniformName, 
			void* value, 
			re::Shader::UniformType const type,
			int const count);
		static void (*SetParameterBlock)(re::Shader& shader, re::ParameterBlock& paramBlock);
		static void (*Destroy)(re::Shader&);
		static void (*LoadShaderTexts)(std::string const& extensionlessName, std::vector<std::string>& shaderTexts_out);
	};
}	

