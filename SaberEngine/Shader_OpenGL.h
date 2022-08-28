#pragma once

#include <memory>
#include <string>
#include <unordered_map>

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

			std::unordered_map<std::string, int32_t> m_samplerUnits;
		};

		static void Create(gr::Shader& shader);
		static void Bind(gr::Shader const& shader, bool doBind);
		static void SetUniform(
			gr::Shader const& shader, char const* uniformName, void const* value, platform::Shader::UNIFORM_TYPE const& type, int count);
		static void Destroy(gr::Shader& shader);

		static void SetTexture(
			gr::Shader const& shader,
			std::string const& shaderName,
			std::shared_ptr<gr::Texture> texture,
			std::shared_ptr<gr::Sampler const> sampler);
	};
}