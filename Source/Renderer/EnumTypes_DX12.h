// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "EnumTypes.h"

#include <d3dx12.h>


namespace dx12
{
	extern constexpr DXGI_FORMAT DataTypeToDXGI_FORMAT(re::DataType, bool isNormalized);
}