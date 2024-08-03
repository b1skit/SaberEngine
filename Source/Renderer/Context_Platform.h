// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"
#include "PipelineState.h"


namespace platform
{
	class Context
	{
	public:
		static void (*Destroy)(re::Context& context);
	};
}