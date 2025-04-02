// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"

#include "Core/Assert.h"

#include "Core/Util/CastUtils.h"



namespace re
{
	struct RootConstant
	{
		std::string m_shaderName;

		re::DataType m_dataType; // Note: Only 32-bit types allowed

		union
		{
			float m_float;
			glm::vec2 m_float2;
			glm::vec3 m_float3;
			glm::vec4 m_float4;

			int m_int;
			glm::ivec2 m_int2;
			glm::ivec3 m_int3;
			glm::ivec4 m_int4;

			uint32_t m_uint;
			glm::uvec2 m_uint2;
			glm::uvec3 m_uint3;
			glm::uvec4 m_uint4;
		};
	};


	class RootConstants
	{
	public:
		void SetRootConstant(char const* shaderName, void const* src, re::DataType); // Max 16B (4x 32-bit values)
		void SetRootConstant(std::string const& shaderName, void const* src, re::DataType); // Max 16B (4x 32-bit values)


	public:
		uint8_t GetRootConstantCount() const;

		std::string const& GetShaderName(uint8_t index) const;
		re::DataType GetDataType(uint8_t index) const;
		void const* GetValue(uint8_t index) const;

		uint64_t GetDataHash() const;


	private:
		std::vector<re::RootConstant> m_rootConstants;
	};


	inline void RootConstants::SetRootConstant(std::string const& shaderName, void const* src, re::DataType dataType)
	{
		SetRootConstant(shaderName.c_str(), src, dataType);
	}


	inline uint8_t RootConstants::GetRootConstantCount() const
	{
		return util::CheckedCast<uint8_t>(m_rootConstants.size());
	}


	inline std::string const& RootConstants::GetShaderName(uint8_t index) const
	{
		SEAssert(index < m_rootConstants.size(), "Index is OOB");

		return m_rootConstants[index].m_shaderName;
	}


	inline re::DataType RootConstants::GetDataType(uint8_t index) const
	{
		return m_rootConstants[index].m_dataType;
	}
}