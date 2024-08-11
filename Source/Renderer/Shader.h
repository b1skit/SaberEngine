// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "PipelineState.h"
#include "VertexStream.h"
#include "VertexStreamMap.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformParams.h"


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
	class VertexStreamMap;


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


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
			bool m_isCreated = false;
		};


	public:
		[[nodiscard]] static std::shared_ptr<re::Shader> GetOrCreate(
			std::vector<std::pair<std::string, ShaderType>> const& extensionlessTypeFilenames,
			re::PipelineState const*,
			re::VertexStreamMap const*);


		~Shader();

		Shader(Shader&&) = default;
		Shader& operator=(Shader&&) = default;
		
		ShaderID GetShaderIdentifier() const;

		bool HasShaderType(ShaderType) const;

		re::PipelineState const* GetPipelineState() const;
			
		inline PlatformParams* GetPlatformParams() const;
		inline void SetPlatformParams(std::unique_ptr<PlatformParams> params);

		uint8_t GetVertexAttributeSlot(re::VertexStream::Type, uint8_t semanticIdx) const;
		re::VertexStreamMap const* GetVertexStreamMap() const;


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
		re::VertexStreamMap const* m_vertexStreamMap;


	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader& operator=(Shader&) = delete;

	private:
		friend class dx12::Shader;
		friend class opengl::Shader;
	};


	inline ShaderID Shader::GetShaderIdentifier() const
	{
		return m_shaderIdentifier;
	}


	inline bool Shader::HasShaderType(ShaderType shaderType) const
	{
		for (auto const& source : m_extensionlessSourceFilenames)
		{
			if (source.second == shaderType)
			{
				return true;
			}
		}
		return false;
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


	inline 	uint8_t Shader::GetVertexAttributeSlot(re::VertexStream::Type streamType, uint8_t semanticIdx) const
	{
		return m_vertexStreamMap->GetSlotIdx(streamType, semanticIdx);
	}


	inline re::VertexStreamMap const* Shader::GetVertexStreamMap() const
	{
		return m_vertexStreamMap;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Shader::PlatformParams::~PlatformParams() {};
}