#pragma once

#include <memory>
#include <vector>

#include "Shader_Platform.h"


namespace gr
{
	class Shader;
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
		};

		static void Create(gr::Shader& shader, std::vector<std::string> const* shaderKeywords);
		static void Bind(gr::Shader const& shader, bool doBind);
		static void SetUniform(
			gr::Shader const& shader, char const* uniformName, void const* value, platform::Shader::UNIFORM_TYPE const& type, int count);
		static void Destroy(gr::Shader& shader);
	};
}