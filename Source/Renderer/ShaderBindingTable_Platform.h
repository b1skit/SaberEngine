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
		static void (*Update)(re::ShaderBindingTable&, uint64_t currentFrameNum);
	};
}