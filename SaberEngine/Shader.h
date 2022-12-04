#pragma once

#include <string>
#include <vector>
#include <memory>

#include "ParameterBlock.h"
#include "NamedObject.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Texture; 
	class Sampler;


	class Shader : public virtual en::NamedObject
	{
	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
		};


	public:
		enum UniformType
		{
			Matrix4x4f,
			Matrix3x3f,
			Vec3f,
			Vec4f,
			Float,
			Int,
			Texture,
			Sampler
		};


	public:
		explicit Shader(std::string const& extensionlessShaderFilename);
		~Shader() { Destroy(); }

		void SetUniform(std::string const&, void* value, UniformType const type, int count);

		void SetParameterBlock(re::ParameterBlock const& parambBlock);
			
		// Helper: Simultaneously calls SetUniform for the texture and sampler
		void SetTextureSamplerUniform(
			std::string const& uniformName, std::shared_ptr<gr::Texture> texture, std::shared_ptr<gr::Sampler> sampler);

		inline PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		inline PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<PlatformParams> params) { m_platformParams = std::move(params); }

		inline std::vector<std::string>& ShaderKeywords() { return m_shaderKeywords; }
		inline std::vector<std::string> const& ShaderKeywords() const { return m_shaderKeywords; }


	private:
		void Destroy();


	private:
		std::unique_ptr<PlatformParams> m_platformParams;

		std::vector<std::string> m_shaderKeywords;

	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader(Shader&&) = delete;
		Shader& operator=(Shader&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Shader::PlatformParams::~PlatformParams() {};
}


