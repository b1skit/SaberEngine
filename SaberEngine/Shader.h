#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Shader_Platform.h"
#include "ParameterBlock.h"
#include "NamedObject.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Shader : public virtual en::NamedObject
	{
	public:
		explicit Shader(std::string const& extensionlessShaderFilename);
		~Shader() { Destroy(); }

		void Create();
		void Bind(bool doBind) const;

		void Destroy();

		void SetUniform(
			std::string const&,
			void const* value,
			platform::Shader::UniformType const type, 
			int count) const;

		void SetParameterBlock(re::ParameterBlock::Handle parambBlock) const;
			
		// Helper: Simultaneously calls SetUniform for the texture and sampler
		void SetTextureSamplerUniform(
			std::string const& uniformName,
			std::shared_ptr<gr::Texture> texture, 
			std::shared_ptr<gr::Sampler const> sampler) const;

		inline platform::Shader::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		inline platform::Shader::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

		inline std::vector<std::string>& ShaderKeywords() { return m_shaderKeywords; }
		inline std::vector<std::string> const& ShaderKeywords() const { return m_shaderKeywords; }

	private:
		std::unique_ptr<platform::Shader::PlatformParams> m_platformParams;

		std::vector<std::string> m_shaderKeywords;

		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Shader::PlatformParams::CreatePlatformParams(gr::Shader&);

	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader(Shader&&) = delete;
		Shader& operator=(Shader&) = delete;
	};
}


