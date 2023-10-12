// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "ParameterBlock.h"
#include "PipelineState.h"
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
		struct PlatformParams : public re::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
			bool m_isCreated = false;
		};


	public: // Object factory: Gets a Shader if it already exists, or creates it if it doesn't
		static std::shared_ptr<re::Shader> Create(
			std::string const& extensionlessShaderFilename, re::PipelineState const&);
		~Shader() { Destroy(); }

		Shader(Shader&&) = default;
		Shader& operator=(Shader&&) = default;
		
		inline bool IsCreated() const;

		re::PipelineState const& GetPipelineState() const;
			
		inline PlatformParams* GetPlatformParams() const;
		inline void SetPlatformParams(std::unique_ptr<PlatformParams> params);


	private:
		explicit Shader(std::string const& extensionlessShaderFilename, re::PipelineState const&);
		void Destroy();


	private:
		std::unique_ptr<PlatformParams> m_platformParams;

		re::PipelineState m_pipelineState;
		

	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader& operator=(Shader&) = delete;
	};


	inline bool Shader::IsCreated() const
	{
		return m_platformParams->m_isCreated;
	}


	inline re::PipelineState const& Shader::GetPipelineState() const
	{
		return m_pipelineState;
	}


	inline Shader::PlatformParams* Shader::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	inline void Shader::SetPlatformParams(std::unique_ptr<PlatformParams> params)
	{
		m_platformParams = std::move(params);
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Shader::PlatformParams::~PlatformParams() {};
}


