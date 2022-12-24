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
			PlatformParams() {}
			~PlatformParams() override {}

			uint32_t m_shaderReference = 0;

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};

		static void Create(re::Shader& shader);
		static void Bind(re::Shader& shader);

		static void SetUniform(
			re::Shader& shader, 
			std::string const& uniformName, 
			void* value, 
			re::Shader::UniformType const type, 
			int const count);
		static void SetParameterBlock(re::Shader&, re::ParameterBlock&);
		static void Destroy(re::Shader& shader);
		static void LoadShaderTexts(std::string const& extensionlessName, std::vector<std::string>& shaderTexts_out);
	};
}