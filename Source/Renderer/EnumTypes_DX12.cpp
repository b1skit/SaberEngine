// © 2024 Adam Badke. All rights reserved.
#include "EnumTypes_DX12.h"

#include <dxgiformat.h>


namespace dx12
{
	constexpr DXGI_FORMAT DataTypeToDXGI_FORMAT(re::DataType dataType, bool isNormalized)
	{
		switch (dataType)
		{
		case re::DataType::Float: return DXGI_FORMAT_R32_FLOAT;
		case re::DataType::Float2: return DXGI_FORMAT_R32G32_FLOAT;
		case re::DataType::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case re::DataType::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;

		case re::DataType::Int: return DXGI_FORMAT_R32_SINT;
		case re::DataType::Int2: return DXGI_FORMAT_R32G32_SINT;
		case re::DataType::Int3: return DXGI_FORMAT_R32G32B32_SINT;
		case re::DataType::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;

		case re::DataType::UInt: return DXGI_FORMAT_R32_UINT;
		case re::DataType::UInt2: return DXGI_FORMAT_R32G32_UINT;
		case re::DataType::UInt3: return DXGI_FORMAT_R32G32B32_UINT;
		case re::DataType::UInt4: return DXGI_FORMAT_R32G32B32A32_UINT;

		case re::DataType::Short:
			return isNormalized ? DXGI_FORMAT_R16_SNORM : DXGI_FORMAT_R16_SINT;
		case re::DataType::Short2:
			return isNormalized ? DXGI_FORMAT_R16G16_SNORM : DXGI_FORMAT_R16G16_SINT;
		case re::DataType::Short4:
			return isNormalized ? DXGI_FORMAT_R16G16B16A16_SNORM : DXGI_FORMAT_R16G16B16A16_SINT;

		case re::DataType::UShort:
			return isNormalized ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R16_UINT;
		case re::DataType::UShort2:
			return isNormalized ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16G16_UINT;
		case re::DataType::UShort4:
			return isNormalized ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R16G16B16A16_UINT;

		case re::DataType::Byte:
			return isNormalized ? DXGI_FORMAT_R8_SNORM : DXGI_FORMAT_R8_SINT;
		case re::DataType::Byte2:
			return isNormalized ? DXGI_FORMAT_R8G8_SNORM : DXGI_FORMAT_R8G8_SINT;
		case re::DataType::Byte4:
			return isNormalized ? DXGI_FORMAT_R8G8B8A8_SNORM : DXGI_FORMAT_R8G8B8A8_SINT;

		case re::DataType::UByte:
			return isNormalized ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8_UINT;
		case re::DataType::UByte2:
			return isNormalized ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8G8_UINT;
		case re::DataType::UByte4:
			return isNormalized ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UINT;
		default: 
			return DXGI_FORMAT_UNKNOWN; // Error
		}
	}
}