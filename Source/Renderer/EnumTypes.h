// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace platform
{
	enum RenderingAPI : uint8_t
	{
		DX12,
		OpenGL,
		RenderingAPI_Count
	};
	extern constexpr char const* RenderingAPIToCStr(platform::RenderingAPI);
}

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


	enum class ViewType : uint8_t
	{
		CBV,
		SRV,
		UAV,
	};


	enum class GeometryMode : uint8_t
	{
		// Note: All draws are instanced, even if an API supports non-instanced drawing
		IndexedInstanced,
		ArrayInstanced,

		Invalid
	};
}