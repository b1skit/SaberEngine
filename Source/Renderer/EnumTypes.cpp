// © 2024 Adam Badke. All rights reserved.
#include "EnumTypes.h"

#include "Core/Assert.h"

#include "Core/Util/HashKey.h"
#include "Core/Util/TextUtils.h"


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


	re::DataType StrToDataType(std::string const& dataTypeStr)
	{
		static const std::unordered_map<util::HashKey const, re::DataType> s_strLowerToDataType =
		{
			{ util::HashKey("float"),	re::DataType::Float },
			{ util::HashKey("float2"),	re::DataType::Float2 },
			{ util::HashKey("float3"),	re::DataType::Float3 },
			{ util::HashKey("float4"),	re::DataType::Float4 },

			{ util::HashKey("int"),		re::DataType::Int },
			{ util::HashKey("int2"),	re::DataType::Int2 },
			{ util::HashKey("int3"),	re::DataType::Int3 },
			{ util::HashKey("int4"),	re::DataType::Int4 },

			{ util::HashKey("uint"),	re::DataType::UInt },
			{ util::HashKey("uint2"),	re::DataType::UInt2 },
			{ util::HashKey("uint3"),	re::DataType::UInt3 },
			{ util::HashKey("uint4"),	re::DataType::UInt4 },

			{ util::HashKey("short"),	re::DataType::Short },
			{ util::HashKey("short2"),	re::DataType::Short2 },
			{ util::HashKey("short4"),	re::DataType::Short4 },

			{ util::HashKey("ushort"),	re::DataType::UShort },
			{ util::HashKey("ushort2"),	re::DataType::UShort2 },
			{ util::HashKey("ushort4"),	re::DataType::UShort4 },

			{ util::HashKey("byte"),	re::DataType::Byte },
			{ util::HashKey("byte2"),	re::DataType::Byte2 },
			{ util::HashKey("byte4"),	re::DataType::Byte4 },

			{ util::HashKey("ubyte"),	re::DataType::UByte },
			{ util::HashKey("ubyte2"),	re::DataType::UByte2 },
			{ util::HashKey("ubyte4"),	re::DataType::UByte4 },
		};
		SEAssert(s_strLowerToDataType.size() == static_cast<size_t>(re::DataType::DataType_Count),
			"Data types are out of sync");

		const util::HashKey dataTypeStrLowerHashkey = util::HashKey::Create(util::ToLower(dataTypeStr));

		SEAssert(s_strLowerToDataType.contains(dataTypeStrLowerHashkey), "Invalid data type name");

		return s_strLowerToDataType.at(dataTypeStrLowerHashkey);
	}
}