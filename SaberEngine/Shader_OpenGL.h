#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Shader.h"
#include "Shader_Platform.h"


namespace gr
{
	class Shader;
}

namespace re
{
	class ParameterBlock;
}

namespace opengl
{
	class Shader
	{
	public:
		struct PlatformParams : public virtual gr::Shader::PlatformParams
		{
			PlatformParams() {}
			~PlatformParams() override {}

			uint32_t m_shaderReference = 0;

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};

		static void Create(gr::Shader& shader);
		static void Bind(gr::Shader& shader, bool doBind);

		static void SetUniform(
			gr::Shader& shader, 
			std::string const& uniformName, 
			void* value, 
			gr::Shader::UniformType const type, 
			int const count);
		static void SetParameterBlock(gr::Shader&, re::ParameterBlock&);
		static void Destroy(gr::Shader& shader);
		static void LoadShaderTexts(std::string const& extensionlessName, std::vector<std::string>& shaderTexts_out);
	};
}