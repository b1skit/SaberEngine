// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "ParameterBlock.h"


namespace dx12
{
	class ParameterBlock
	{
	public:
		struct PlatformParams final : public virtual re::ParameterBlock::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;
		};


	public:
		// Platform handles:
		static void Create(re::ParameterBlock& paramBlock);
		static void Update(re::ParameterBlock& paramBlock);
		static void Destroy(re::ParameterBlock& paramBlock);
	};
}