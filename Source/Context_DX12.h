// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"


namespace dx12
{
	class Context
	{
	public:
		struct PlatformParams final : public virtual re::Context::PlatformParams
		{

		};


	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void Present(re::Context const& context);
		static void SetVSyncMode(re::Context const& window, bool enabled);
		static void SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState);
		static uint32_t GetMaxTextureInputs();
	};
}