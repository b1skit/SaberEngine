// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "ShaderBindingTable.h"


namespace platform
{
	class ShaderBindingTable
	{
	public:
		static std::unique_ptr<re::ShaderBindingTable::PlatformParams> CreatePlatformParams();


	public:
		static void (*Create)(re::ShaderBindingTable&);
	};
}