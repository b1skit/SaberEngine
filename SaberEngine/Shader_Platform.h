#pragma once

#include "Shader.h"


namespace gr
{
	class Shader;
	class Texture;
	class Sampler;
}

namespace re
{
	class ParameterBlock;
}

namespace platform
{
	class Shader
	{
	public:
		static void CreatePlatformParams(gr::Shader& shader);

		// Static helpers:
		static std::string LoadShaderText(const std::string& filepath); // Loads file "filepath" within the shaders dir
		// TODO: Move this function to the util namespace, as a generic text loader

		static void	InsertIncludedFiles(std::string& shaderText);
		static void	InsertDefines(std::string& shaderText, std::vector<std::string> const* shaderKeywords);
		

		// Static pointers:
		static void (*Create)(gr::Shader& shader);
		static void (*Bind)(gr::Shader&, bool doBind);
		static void (*SetUniform)(
			gr::Shader& shader, 
			std::string const& uniformName, 
			void* value, 
			gr::Shader::UniformType const type,
			int const count);
		static void (*SetParameterBlock)(gr::Shader& shader, re::ParameterBlock const& paramBlock);
		static void (*Destroy)(gr::Shader&);
	};
}	

