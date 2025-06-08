// © 2025 Adam Badke. All rights reserved.
#include "Private/RootConstants.h"

#include "Core/Util/HashUtils.h"


namespace re
{
	void RootConstants::SetRootConstant(char const* shaderName, void const* src, re::DataType dataType)
	{
		RootConstant* dest = nullptr;

		bool foundExisting = false;
		for (auto& existing : m_rootConstants)
		{
			if (strcmp(existing.m_shaderName.c_str(), shaderName) == 0)
			{
				SEAssert(existing.m_dataType == dataType,
					"Root constant with the given name already exists, but with a different data type");

				dest = &existing;

				foundExisting = true;
				break;
			}
		}

		if (!foundExisting)
		{
			dest = &m_rootConstants.emplace_back(RootConstant{
				.m_shaderName = shaderName,
				.m_dataType = dataType,
				});
		}

		const uint8_t numBytes = re::DataTypeToByteStride(dataType);

		switch (dataType)
		{
		case re::DataType::Float: memcpy(&dest->m_float, src, numBytes); break;
		case re::DataType::Float2: memcpy(&dest->m_float2, src, numBytes); break;
		case re::DataType::Float3: memcpy(&dest->m_float3, src, numBytes); break;
		case re::DataType::Float4: memcpy(&dest->m_float4, src, numBytes); break;

		case re::DataType::Int: memcpy(&dest->m_int, src, numBytes); break;
		case re::DataType::Int2: memcpy(&dest->m_int2, src, numBytes); break;
		case re::DataType::Int3: memcpy(&dest->m_int3, src, numBytes); break;
		case re::DataType::Int4: memcpy(&dest->m_int4, src, numBytes); break;

		case re::DataType::UInt: memcpy(&dest->m_uint, src, numBytes); break;
		case re::DataType::UInt2: memcpy(&dest->m_uint2, src, numBytes); break;
		case re::DataType::UInt3: memcpy(&dest->m_uint3, src, numBytes); break;
		case re::DataType::UInt4: memcpy(&dest->m_uint4, src, numBytes); break;

		default: SEAssertF("Invalid/unsupported data type for root constants");
		}
	}


	void const* RootConstants::GetValue(uint8_t index) const
	{
		SEAssert(index < m_rootConstants.size(), "Index is OOB");

		switch (m_rootConstants[index].m_dataType)
		{
		case re::DataType::Float: return &m_rootConstants[index].m_float;
		case re::DataType::Float2: return &m_rootConstants[index].m_float2;
		case re::DataType::Float3: return &m_rootConstants[index].m_float3;
		case re::DataType::Float4: return &m_rootConstants[index].m_float4;

		case re::DataType::Int: return &m_rootConstants[index].m_int;
		case re::DataType::Int2: return &m_rootConstants[index].m_int2;
		case re::DataType::Int3: return &m_rootConstants[index].m_int3;
		case re::DataType::Int4: return &m_rootConstants[index].m_int4;

		case re::DataType::UInt: return &m_rootConstants[index].m_uint;
		case re::DataType::UInt2: return &m_rootConstants[index].m_int2;
		case re::DataType::UInt3: return &m_rootConstants[index].m_int3;
		case re::DataType::UInt4: return &m_rootConstants[index].m_int4;

		default: SEAssertF("Invalid/unsupported data type for root constants");
		}
		return nullptr; // This should never happen
	}


	uint64_t RootConstants::GetDataHash() const
	{
		uint64_t hashResult = 0;

		util::AddDataBytesToHash(hashResult, m_rootConstants.size());

		for (auto const& rootConstant : m_rootConstants)
		{
			util::AddDataBytesToHash(hashResult, rootConstant.m_shaderName);
			util::AddDataBytesToHash(hashResult, rootConstant.m_dataType);

			switch (rootConstant.m_dataType)
			{
			case re::DataType::Float: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_float, 4)); break;
			case re::DataType::Float2: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_float2, 8)); break;
			case re::DataType::Float3: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_float3, 12)); break;
			case re::DataType::Float4: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_float4, 16)); break;

			case re::DataType::Int: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_int, 4)); break;
			case re::DataType::Int2: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_int2, 8)); break;
			case re::DataType::Int3: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_int3, 12)); break;
			case re::DataType::Int4: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_int4, 16)); break;

			case re::DataType::UInt: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_uint, 4)); break;
			case re::DataType::UInt2: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_uint2, 8)); break;
			case re::DataType::UInt3: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_uint3, 12)); break;
			case re::DataType::UInt4: util::CombineHash(hashResult, util::HashDataBytes(&rootConstant.m_uint4, 16)); break;

			default: SEAssertF("Invalid/unsupported data type for root constants");
			}
		}

		return hashResult;
	}
}