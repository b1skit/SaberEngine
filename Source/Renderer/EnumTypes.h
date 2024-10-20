// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	enum class Lifetime : bool
	{
		SingleFrame,
		Permanent
	};


	enum class DataType : uint8_t
	{
		Float,	// 32-bit
		Float2,
		Float3,
		Float4,

		Int,	// 32-bit
		Int2,
		Int3,
		Int4,

		UInt,	// 32-bit
		UInt2,
		UInt3,
		UInt4,

		Short,	// 16-bit
		Short2,
		Short4,

		UShort,	// 16-bit
		UShort2,
		UShort4,

		Byte,	// 8-bit
		Byte2,
		Byte4,

		UByte,	// 8-bit
		UByte2,
		UByte4,

		DataType_Count
	};
	extern constexpr char const* DataTypeToCStr(DataType);
	extern constexpr uint8_t DataTypeToNumComponents(DataType);
	extern constexpr uint8_t DataTypeToComponentByteSize(DataType);
	extern constexpr uint8_t DataTypeToByteStride(DataType);
	extern re::DataType StrToDataType(std::string const& dataTypeStr);
}