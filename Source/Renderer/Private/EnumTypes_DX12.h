// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Private/EnumTypes.h"


enum DXGI_FORMAT;

namespace dx12
{
	extern constexpr DXGI_FORMAT DataTypeToDXGI_FORMAT(re::DataType, bool isNormalized);
}