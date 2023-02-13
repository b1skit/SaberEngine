// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Shader.h"
#include "Shader_Platform.h"


namespace re
{
	class ParameterBlock;
	class Shader;
}

namespace opengl
{
	class Shader
	{
	public:
		struct PlatformParams final : public virtual re::Shader::PlatformParams
		{
			uint32_t m_shaderReference = 0;

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};


	public:
		static void Create(re::Shader& shader);
		static void Destroy(re::Shader& shader);

		static void LoadShaderTexts(re::Shader&);

		// OpenGL-specific functions:
		static void Bind(re::Shader& shader);
		static void SetUniform(
			re::Shader& shader, 
			std::string const& uniformName, 
			void* value, 
			re::Shader::UniformType const type, 
			int const count);
		static void SetTextureAndSampler(re::Shader&, std::string const& uniformName, std::shared_ptr<re::Texture>, std::shared_ptr<re::Sampler>);
		static void SetParameterBlock(re::Shader&, re::ParameterBlock&); // TODO: This Shader& can probably be const
	};
}