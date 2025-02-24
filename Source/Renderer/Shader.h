// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RasterizationState.h"
#include "VertexStream.h"
#include "VertexStreamMap.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformParams.h"

#include "Core/Util/HashKey.h"


namespace core
{
	template<typename T>
	class InvPtr;
}
namespace dx12
{
	class Shader;
}
namespace opengl
{
	class Shader;
}

using ShaderID = util::HashKey;

namespace re
{
	class Shader final : public virtual core::INamedObject
	{
	public:
		enum ShaderType : uint8_t
		{
			// Rasterization pipeline:
			Vertex,
			Geometry,
			Pixel,

			Hull,			// OpenGL: Tesselation Control Shader
			Domain,			// OpenGL: Tesselation Evaluation Shader

			// Mesh shading pipeline:
			Amplification,  // Not (currently) supported on OpenGL
			Mesh,			// Not (currently) supported on OpenGL

			// Compute pipeline:
			Compute,

			ShaderType_Count
		};
		static constexpr char const* ShaderTypeToCStr(ShaderType);


	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = default;
			bool m_isCreated = false;
		};


	public:
		[[nodiscard]] static core::InvPtr<re::Shader> GetOrCreate(
			std::vector<std::pair<std::string, ShaderType>> const& extensionlessTypeFilenames,
			re::RasterizationState const*,
			re::VertexStreamMap const*);

		Shader(Shader&&) noexcept = default;
		Shader& operator=(Shader&&) noexcept = default;

		~Shader();

		void Destroy();


	public:		
		ShaderID GetShaderIdentifier() const;

		bool HasShaderType(ShaderType) const;

		re::RasterizationState const* GetRasterizationState() const;
			
		inline PlatformParams* GetPlatformParams() const;
		inline void SetPlatformParams(std::unique_ptr<PlatformParams> params);

		uint8_t GetVertexAttributeSlot(gr::VertexStream::Type, uint8_t semanticIdx) const;
		re::VertexStreamMap const* GetVertexStreamMap() const;


	private:
		explicit Shader(
			std::string const& shaderName,
			std::vector<std::pair<std::string, ShaderType>> const& extensionlessTypeFilenames, 
			re::RasterizationState const*,
			re::VertexStreamMap const*,
			uint64_t shaderIdentifier);


	private:
		const ShaderID m_shaderIdentifier;
		std::vector<std::pair<std::string, ShaderType>> m_extensionlessSourceFilenames;

		std::unique_ptr<PlatformParams> m_platformParams;

		re::RasterizationState const* m_rasterizationState;
		re::VertexStreamMap const* m_vertexStreamMap;


	private:
		Shader() = delete;
		Shader(Shader const&) = delete;
		Shader& operator=(Shader&) = delete;

	private:
		friend class dx12::Shader;
		friend class opengl::Shader;
	};


	inline constexpr char const* Shader::ShaderTypeToCStr(ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Vertex: return "Vertex";
		case re::Shader::ShaderType::Geometry: return "Geometry";
		case re::Shader::ShaderType::Pixel: return "Pixel";
		case re::Shader::ShaderType::Hull: return "Hull";
		case re::Shader::ShaderType::Domain: return "Domain";
		case re::Shader::ShaderType::Mesh: return "Mesh";
		case re::Shader::ShaderType::Amplification: return "Amplification";
		case re::Shader::ShaderType::Compute: return "Compute";
		default: return "INVALID_SHADER_TYPE_RECEIVED";
		}
	}


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


	inline re::RasterizationState const* Shader::GetRasterizationState() const
	{
		return m_rasterizationState;
	}


	inline Shader::PlatformParams* Shader::GetPlatformParams() const
	{
		return m_platformParams.get();
	}


	inline void Shader::SetPlatformParams(std::unique_ptr<PlatformParams> params)
	{
		m_platformParams = std::move(params);
	}


	inline uint8_t Shader::GetVertexAttributeSlot(gr::VertexStream::Type streamType, uint8_t semanticIdx) const
	{
		return m_vertexStreamMap->GetSlotIdx(streamType, semanticIdx);
	}


	inline re::VertexStreamMap const* Shader::GetVertexStreamMap() const
	{
		return m_vertexStreamMap;
	}
}