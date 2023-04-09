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

		inline bool IsCreated() const;
			
		inline PlatformParams* GetPlatformParams() const;
		inline void SetPlatformParams(std::unique_ptr<PlatformParams> params);

		inline std::vector<std::string>& ShaderKeywords();
		inline std::vector<std::string> const& ShaderKeywords() const;


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


	bool Shader::IsCreated() const
	{
		return m_platformParams->m_isCreated;
	}


	Shader::PlatformParams* Shader::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	void Shader::SetPlatformParams(std::unique_ptr<PlatformParams> params)
	{
		m_platformParams = std::move(params);
	}


	std::vector<std::string>& Shader::ShaderKeywords() 
	{ 
		return m_shaderKeywords; 
	}


	std::vector<std::string> const& Shader::ShaderKeywords() const 
	{ 
		return m_shaderKeywords; 
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Shader::PlatformParams::~PlatformParams() {};
}


