// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Context.h"


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
		static void (*SetCullingMode)(re::Context::FaceCullingMode const& mode);
		static void (*ClearTargets)(re::Context::ClearTarget const& clearTarget);
		static void (*SetBlendMode)(re::Context::BlendMode const& src, re::Context::BlendMode const& dst);
		static void (*SetDepthTestMode)(re::Context::DepthTestMode const& mode);
		static void (*SetDepthWriteMode)(re::Context::DepthWriteMode const& mode);
		static void (*SetColorWriteMode)(re::Context::ColorWriteMode const& channelModes);
		static uint32_t(*GetMaxTextureInputs)();
	};
}