// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"
#include "PipelineState.h"


namespace platform
{
	class Context
	{
	public:
		// API-specific function pointers:
		static void CreatePlatformParams(re::Context& m_context);

	public:
		// Static function pointers:
		static void (*Create)(re::Context& context);
		static void (*Destroy)(re::Context& context);
		static void (*Present)(re::Context const& context);
		static void (*SetVSyncMode)(re::Context const& window, bool enabled);
		static void (*SetPipelineState)(re::Context const&, gr::PipelineState const& pipelineState);
		static uint8_t(*GetMaxTextureInputs)();
		static uint8_t(*GetMaxColorTargets)();
	};
}