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

		static std::string LoadShaderText(const std::string& filepath); // Loads file "filepath" within the shaders dir

		// TODO: These helpers probably belong in the OpenGL namespace
		static void	InsertIncludedFiles(std::string& shaderText);
		static void	InsertDefines(std::string& shaderText, std::vector<std::string> const* shaderKeywords);
		
	public: // Api-specific functionality
		static void (*Create)(re::Shader&);
		static void (*Destroy)(re::Shader&);
	};
}	

