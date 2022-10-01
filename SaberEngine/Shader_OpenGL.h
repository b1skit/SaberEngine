#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Shader_Platform.h"


namespace gr
{
	class Shader;
}

namespace re
{
	class PermanentParameterBlock;
}

namespace opengl
{
	class Shader
	{
	public:
		struct PlatformParams : public virtual platform::Shader::PlatformParams
		{
			PlatformParams() {}
			~PlatformParams() override {}

			uint32_t m_shaderReference = 0;

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};

		static void Create(gr::Shader& shader);
		static void Bind(gr::Shader const& shader, bool doBind);
		static void SetUniform(
			gr::Shader const& shader, 
			std::string const& uniformName, 
			void const* value, 
			platform::Shader::UniformType const type, 
			int const count);
		static void SetParameterBlock(gr::Shader const&, re::PermanentParameterBlock const&);
		static void Destroy(gr::Shader& shader);
	};
}