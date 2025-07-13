// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "RasterState.h"
#include "VertexStream.h"
#include "VertexStreamMap.h"

#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IPlatformObject.h"

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
		enum class PipelineType
		{
			Rasterization,
			Mesh,
			Compute,
			RayTracing,

			Invalid
		};

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

			// Ray tracing pipeline:
			HitGroup_Intersection,	// If defined: Procedural geometry only. Otherwise, triangle geometry only
			HitGroup_AnyHit,		// If not defined, all geo is considered opaque
			HitGroup_ClosestHit,

			Callable,
			RayGen,
			Miss,

			ShaderType_Count
		};
		static constexpr char const* ShaderTypeToCStr(ShaderType);
		static constexpr bool IsRasterizationType(ShaderType);
		static constexpr bool IsMeshShadingType(ShaderType);
		static constexpr bool IsComputeType(ShaderType);
		static constexpr bool IsRayTracingType(ShaderType);
		static constexpr bool IsRayTracingHitGroupType(ShaderType);
		static constexpr bool IsSamePipelineType(ShaderType, ShaderType);
		static constexpr PipelineType ShaderTypeToPipelineType(ShaderType shaderType);


	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;
			bool m_isCreated = false;
		};


	public:
		struct Metadata final
		{
			std::string m_extensionlessFilename;
			std::string m_entryPoint;
			ShaderType m_type;
		};
		[[nodiscard]] static core::InvPtr<re::Shader> GetOrCreate(
			std::vector<Metadata> const& metadata,
			re::RasterState const*,
			re::VertexStreamMap const*);

		Shader(Shader&&) noexcept = default;
		Shader& operator=(Shader&&) noexcept = default;

		~Shader();

		void Destroy();


	public:		
		ShaderID GetShaderIdentifier() const;

		PipelineType GetPipelineType() const;

		re::RasterState const* GetRasterizationState() const;
			
		inline PlatObj* GetPlatformObject() const;
		inline void SetPlatformObject(std::unique_ptr<PlatObj>);

		uint8_t GetVertexAttributeSlot(re::VertexStream::Type, uint8_t semanticIdx) const;
		re::VertexStreamMap const* GetVertexStreamMap() const;

		std::vector<Metadata> const& GetMetadata() const;


	private:
		explicit Shader(
			std::string const& shaderName,
			std::vector<Metadata> const&,
			re::RasterState const*,
			re::VertexStreamMap const*,
			ShaderID);


	private:
		const ShaderID m_shaderIdentifier;
		std::vector<Metadata> m_metadata;
		PipelineType m_pipelineType;

		std::unique_ptr<PlatObj> m_platObj;

		re::RasterState const* m_rasterizationState;
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
		case re::Shader::ShaderType::Amplification: return "Amplification";
		case re::Shader::ShaderType::Mesh: return "Mesh";
		case re::Shader::ShaderType::Compute: return "Compute";
		case re::Shader::ShaderType::HitGroup_Intersection: return "HitGroup_Intersection";
		case re::Shader::ShaderType::HitGroup_AnyHit: return "HitGroup_AnyHit";
		case re::Shader::ShaderType::HitGroup_ClosestHit: return "HitGroup_ClosestHit";
		case re::Shader::ShaderType::Callable: return "Callable";
		case re::Shader::ShaderType::RayGen: return "RayGen";
		case re::Shader::ShaderType::Miss: return "Miss";
		default: return "INVALID_SHADER_TYPE_RECEIVED";
		}
		SEStaticAssert(re::Shader::ShaderType::ShaderType_Count == 14, "Updated this if shader type count has changed");
	}


	inline constexpr bool Shader::IsRasterizationType(ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Vertex:
		case re::Shader::ShaderType::Geometry:
		case re::Shader::ShaderType::Pixel:
		case re::Shader::ShaderType::Hull:
		case re::Shader::ShaderType::Domain:
			return true;
		default: return false;
		}
	}


	inline constexpr bool Shader::IsMeshShadingType(ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::Amplification:
		case re::Shader::ShaderType::Mesh:
			return true;
		default: return false;
		}
	}


	inline constexpr bool Shader::IsComputeType(ShaderType shaderType)
	{
		return shaderType == ShaderType::Compute;
	}


	inline constexpr bool Shader::IsRayTracingType(ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::HitGroup_Intersection:
		case re::Shader::ShaderType::HitGroup_AnyHit:
		case re::Shader::ShaderType::HitGroup_ClosestHit:
		case re::Shader::ShaderType::Callable:
		case re::Shader::ShaderType::RayGen:
		case re::Shader::ShaderType::Miss:
			return true;
		default: return false;
		}
	}


	inline constexpr bool Shader::IsRayTracingHitGroupType(ShaderType shaderType)
	{
		switch (shaderType)
		{
		case re::Shader::ShaderType::HitGroup_Intersection:
		case re::Shader::ShaderType::HitGroup_AnyHit:
		case re::Shader::ShaderType::HitGroup_ClosestHit:
			return true;
		default: return false;
		}
	}


	inline constexpr bool Shader::IsSamePipelineType(ShaderType lhs, ShaderType rhs)
	{
		switch (lhs)
		{
		case re::Shader::ShaderType::Vertex:
		case re::Shader::ShaderType::Geometry:
		case re::Shader::ShaderType::Pixel:
		case re::Shader::ShaderType::Hull:
		case re::Shader::ShaderType::Domain:
			return IsRasterizationType(rhs);
		case re::Shader::ShaderType::Amplification:
		case re::Shader::ShaderType::Mesh:
			return IsMeshShadingType(rhs);
		case re::Shader::ShaderType::Compute:
			return IsComputeType(rhs);
		case re::Shader::ShaderType::HitGroup_Intersection:
		case re::Shader::ShaderType::HitGroup_AnyHit:
		case re::Shader::ShaderType::HitGroup_ClosestHit:
		case re::Shader::ShaderType::Callable:
		case re::Shader::ShaderType::RayGen:
		case re::Shader::ShaderType::Miss:
			return IsRayTracingType(rhs);
		default: return false;
		}
		SEStaticAssert(re::Shader::ShaderType::ShaderType_Count == 14, "Updated this if shader type count has changed");
	}


	inline constexpr Shader::PipelineType Shader::ShaderTypeToPipelineType(ShaderType shaderType)
	{
		if (IsRasterizationType(shaderType))
		{
			return PipelineType::Rasterization;
		}
		else if (IsMeshShadingType(shaderType))
		{
			return PipelineType::Mesh;
		}
		else if (IsComputeType(shaderType))
		{
			return PipelineType::Compute;
		}
		else if (IsRayTracingType(shaderType))
		{
			return PipelineType::RayTracing;
		}
		SEAssertF("Invalid shader type received");
		return PipelineType::Invalid;
	}


	inline ShaderID Shader::GetShaderIdentifier() const
	{
		return m_shaderIdentifier;
	}


	inline Shader::PipelineType Shader::GetPipelineType() const
	{
		return m_pipelineType;
	}


	inline re::RasterState const* Shader::GetRasterizationState() const
	{
		return m_rasterizationState;
	}


	inline Shader::PlatObj* Shader::GetPlatformObject() const
	{
		return m_platObj.get();
	}


	inline void Shader::SetPlatformObject(std::unique_ptr<PlatObj> platObj)
	{
		m_platObj = std::move(platObj);
	}


	inline uint8_t Shader::GetVertexAttributeSlot(re::VertexStream::Type streamType, uint8_t semanticIdx) const
	{
		return m_vertexStreamMap->GetSlotIdx(streamType, semanticIdx);
	}


	inline re::VertexStreamMap const* Shader::GetVertexStreamMap() const
	{
		return m_vertexStreamMap;
	}


	inline std::vector<Shader::Metadata> const& Shader::GetMetadata() const
	{
		return m_metadata;
	}
}