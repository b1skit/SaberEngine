// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Private/ShaderBindingTable.h"


namespace platform
{
	class ShaderBindingTable
	{
	public:
		static std::unique_ptr<re::ShaderBindingTable::PlatObj> CreatePlatformObject();


	public:
		static void (*Create)(re::ShaderBindingTable&);
	};
}