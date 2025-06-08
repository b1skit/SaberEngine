// © 2024 Adam Badke. All rights reserved.
#include "EnumTypes.h"

#include "Core/Assert.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/TextUtils.h"


namespace platform
{
	constexpr char const* RenderingAPIToCStr(platform::RenderingAPI renderingAPI)
	{
		switch (renderingAPI)
		{
		case platform::RenderingAPI::OpenGL: return "OpenGL";
		case platform::RenderingAPI::DX12: return "DX12";
		default: return "platform::RenderingAPIToCStr: Invalid platform::RenderingAPI received";
		}
	}
}

namespace re
{
	constexpr char const* DataTypeToCStr(re::DataType dataType)
	{
		switch (dataType)
		{
		case re::DataType::Float: return "Float";
		case re::DataType::Float2: return "Float2";
		case re::DataType::Float3: return "Float3";
		case re::DataType::Float4: return "Float4";

		case re::DataType::Int: return "Int";
		case re::DataType::Int2: return "Int2";
		case re::DataType::Int3: return "Int3";
		case re::DataType::Int4: return "Int4";

		case re::DataType::UInt: return "UInt";
		case re::DataType::UInt2: return "UInt2";
		case re::DataType::UInt3: return "UInt3";
		case re::DataType::UInt4: return "UInt4";

		case re::DataType::Short: return "Short";
		case re::DataType::Short2: return "Short2";
		case re::DataType::Short4: return "Short4";

		case re::DataType::UShort: return "UShort";
		case re::DataType::UShort2: return "UShort2";
		case re::DataType::UShort4: return "UShort4";

		case re::DataType::Byte: return "Byte";
		case re::DataType::Byte2: return "Byte2";
		case re::DataType::Byte4: return "Byte4";

		case re::DataType::UByte: return "UByte";
		case re::DataType::UByte2: return "UByte2";
		case re::DataType::UByte4: return "UByte4";
		default: return "INVALID_DATA_TYPE";
		}
	}


	constexpr uint8_t DataTypeToNumComponents(DataType dataType)
	{
		switch (dataType)
		{
		case DataType::Float:
		case DataType::UInt:
		case DataType::UShort:
		case DataType::UByte:
			return 1;
		case DataType::Float2:
		case DataType::UInt2:
		case DataType::UShort2:
		case DataType::UByte2:
			return 2;
		case DataType::Float3:
		case DataType::UInt3:
			return 3;
		case DataType::Float4:
		case DataType::UInt4:
		case DataType::UShort4:
		case DataType::UByte4:
			return 4;
		default: return std::numeric_limits<uint8_t>::max(); // Error
		}
	}


	constexpr uint8_t DataTypeToComponentByteSize(DataType dataType)
	{
		switch (dataType)
		{
		case DataType::Float:	// 32-bit
		case DataType::Float2:
		case DataType::Float3:
		case DataType::Float4:

		case DataType::Int:		// 32-bit
		case DataType::Int2:
		case DataType::Int3:
		case DataType::Int4:

		case DataType::UInt:	// 32-bit
		case DataType::UInt2:
		case DataType::UInt3:
		case DataType::UInt4:
			return 4;

		case DataType::Short:	// 16-bit
		case DataType::Short2:
		case DataType::Short4:

		case DataType::UShort:	// 16-bit
		case DataType::UShort2:
		case DataType::UShort4:
			return 2;

		case DataType::Byte:	// 8-bit
		case DataType::Byte2:
		case DataType::Byte4:

		case DataType::UByte:	// 8-bit
		case DataType::UByte2:
		case DataType::UByte4:
			return 1;
		default: return 0; // Error
		}
	}


	constexpr uint8_t DataTypeToByteStride(DataType dataType)
	{
		return DataTypeToComponentByteSize(dataType) * DataTypeToNumComponents(dataType);
	}


	re::DataType StrToDataType(std::string const& dataTypeStr)
	{
		static const std::unordered_map<util::CHashKey, re::DataType> s_strLowerToDataType =
		{
			{ util::CHashKey("float"),	re::DataType::Float },
			{ util::CHashKey("float2"),	re::DataType::Float2 },
			{ util::CHashKey("float3"),	re::DataType::Float3 },
			{ util::CHashKey("float4"),	re::DataType::Float4 },

			{ util::CHashKey("int"),		re::DataType::Int },
			{ util::CHashKey("int2"),	re::DataType::Int2 },
			{ util::CHashKey("int3"),	re::DataType::Int3 },
			{ util::CHashKey("int4"),	re::DataType::Int4 },

			{ util::CHashKey("uint"),	re::DataType::UInt },
			{ util::CHashKey("uint2"),	re::DataType::UInt2 },
			{ util::CHashKey("uint3"),	re::DataType::UInt3 },
			{ util::CHashKey("uint4"),	re::DataType::UInt4 },

			{ util::CHashKey("short"),	re::DataType::Short },
			{ util::CHashKey("short2"),	re::DataType::Short2 },
			{ util::CHashKey("short4"),	re::DataType::Short4 },

			{ util::CHashKey("ushort"),	re::DataType::UShort },
			{ util::CHashKey("ushort2"),	re::DataType::UShort2 },
			{ util::CHashKey("ushort4"),	re::DataType::UShort4 },

			{ util::CHashKey("byte"),	re::DataType::Byte },
			{ util::CHashKey("byte2"),	re::DataType::Byte2 },
			{ util::CHashKey("byte4"),	re::DataType::Byte4 },

			{ util::CHashKey("ubyte"),	re::DataType::UByte },
			{ util::CHashKey("ubyte2"),	re::DataType::UByte2 },
			{ util::CHashKey("ubyte4"),	re::DataType::UByte4 },
		};
		SEAssert(s_strLowerToDataType.size() == static_cast<size_t>(re::DataType::DataType_Count),
			"Data types are out of sync");

		const util::CHashKey dataTypeStrLowerHashkey = util::CHashKey::Create(util::ToLower(dataTypeStr));

		SEAssert(s_strLowerToDataType.contains(dataTypeStrLowerHashkey), "Invalid data type name");

		return s_strLowerToDataType.at(dataTypeStrLowerHashkey);
	}
}