#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Shader_Platform.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Shader
	{
	public:
		// Shader #define keywords
		enum ShaderKeywordIndex
		{
			NoAlbedoTex,
			NoNormalTex,
			NoEmissiveTex,
			NoRMAOTex,
			NoCosinePower,

			ShaderKeyword_Count
		}; // Note: If new enums are added, don't forget to update Shader::k_MatTexNames as well
		
		const static std::vector<std::string> k_ShaderKeywords;


	public:
		Shader(std::string const& shaderName);
		~Shader() { Destroy(); }

		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader(Shader&&) = delete;
		Shader& operator=(Shader&) = delete;

		void Create(std::vector<std::string> const* shaderKeywords);
		void Create();
		void Bind(bool doBind);

		void Destroy();

		// Getters/Setters:
		inline std::string const& Name() { return m_shaderName; }

		// todo: remove default value
		void SetUniform(
			char const* uniformName,
			void const* value,
			platform::Shader::UNIFORM_TYPE const& type, 
			int count = 1);

		platform::Shader::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::Shader::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }


	private:
		// Extensionless shader filename. Will have .vert/.geom.frag appended (thus all shader text must have the
		// same extensionless name)
		std::string m_shaderName;

		std::unique_ptr<platform::Shader::PlatformParams> m_platformParams;


		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Shader::PlatformParams::CreatePlatformParams(gr::Shader&);
	};
}


