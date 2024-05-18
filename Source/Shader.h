// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "PipelineState.h"

#include "Core\Interfaces\INamedObject.h"
#include "Core\Interfaces\IPlatformParams.h"

namespace dx12
{
	class Shader;
}
namespace opengl
{
	class Shader;
}

using ShaderID = uint64_t;

namespace re
{
	class Shader final : public virtual core::INamedObject
	{
	public:
		enum ShaderType : uint8_t
		{
			Vertex,
			Geometry,
			Pixel,

			Hull,			// OpenGL: Tesselation Control Shader (.tesc)
			Domain,			// OpenGL: Tesselation Evaluation Shader (.tese)

			Mesh,			// Not (currently) supported on OpenGL
			Amplification,  // Not (currently) supported on OpenGL

			Compute,

			ShaderType_Count
		};
		static constexpr std::array<char const*, ShaderType_Count> k_shaderTypeNames = {
			ENUM_TO_STR(Vertex),
			ENUM_TO_STR(Geometry),
			ENUM_TO_STR(Pixel),

			ENUM_TO_STR(Hull),
			ENUM_TO_STR(Domain),

			ENUM_TO_STR(Mesh),
			ENUM_TO_STR(Amplification),

			ENUM_TO_STR(Compute),
		};

	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
			bool m_isCreated = false;
		};


	public:
		[[nodiscard]] static std::shared_ptr<re::Shader> GetOrCreate(
			std::vector<std::pair<std::string, ShaderType>> const& extensionlessTypeFilenames,
			re::PipelineState const*);


		~Shader();

		Shader(Shader&&) = default;
		Shader& operator=(Shader&&) = default;
		
		uint64_t GetShaderIdentifier() const;

		inline bool IsCreated() const;

		re::PipelineState const* GetPipelineState() const;
			
		inline PlatformParams* GetPlatformParams() const;
		inline void SetPlatformParams(std::unique_ptr<PlatformParams> params);


	private:
		explicit Shader(
			std::string const& shaderName,
			std::vector<std::pair<std::string, ShaderType>> const& extensionlessTypeFilenames, 
			re::PipelineState const*,
			uint64_t shaderIdentifier);


	private:
		const uint64_t m_shaderIdentifier;
		std::vector<std::pair<std::string, ShaderType>> m_extensionlessSourceFilenames;

		std::unique_ptr<PlatformParams> m_platformParams;

		re::PipelineState const* m_pipelineState;
		

	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader& operator=(Shader&) = delete;

	private:
		friend class dx12::Shader;
		friend class opengl::Shader;
	};


	inline uint64_t Shader::GetShaderIdentifier() const
	{
		return m_shaderIdentifier;
	}


	inline bool Shader::IsCreated() const
	{
		return m_platformParams->m_isCreated;
	}


	inline re::PipelineState const* Shader::GetPipelineState() const
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


