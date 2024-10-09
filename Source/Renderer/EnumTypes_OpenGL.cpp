// © 2024 Adam Badke. All rights reserved.
#include "EnumTypes_OpenGL.h"


namespace opengl
{
	constexpr GLenum DataTypeToGLDataType(re::DataType dataType)
	{
		switch (dataType)
		{
		case re::DataType::Float:	// 32-bit
		case re::DataType::Float2:
		case re::DataType::Float3:
		case re::DataType::Float4:
			return GL_FLOAT;

		case re::DataType::Int:		// 32-bit
		case re::DataType::Int2:
		case re::DataType::Int3:
		case re::DataType::Int4:
			return GL_INT;

		case re::DataType::UInt:	// 32-bit
		case re::DataType::UInt2:
		case re::DataType::UInt3:
		case re::DataType::UInt4:
			return GL_UNSIGNED_INT;

		case re::DataType::Short:	// 16-bit
		case re::DataType::Short2:
		case re::DataType::Short4:
			return GL_SHORT;

		case re::DataType::UShort:	// 16-bit
		case re::DataType::UShort2:
		case re::DataType::UShort4:
			return GL_UNSIGNED_SHORT;

		case re::DataType::Byte:	// 8-bit
		case re::DataType::Byte2:
		case re::DataType::Byte4:
			return GL_BYTE;

		case re::DataType::UByte:	// 8-bit
		case re::DataType::UByte2:
		case re::DataType::UByte4:
			return GL_UNSIGNED_BYTE;
		default: return GL_INVALID_ENUM; // Error
		}
	}
}