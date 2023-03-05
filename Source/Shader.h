// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "ParameterBlock.h"
#include "NamedObject.h"


namespace opengl
{
	class Shader;
}

namespace re
{
	class Shader final : public virtual en::NamedObject
	{
	public:
		struct PlatformParams : public IPlatformParams
		{
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
		};


	public:
		explicit Shader(std::string const& extensionlessShaderFilename);
		~Shader() { Destroy(); }
			
		inline PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
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


