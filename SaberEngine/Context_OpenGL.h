#pragma once

#include "Context_Platform.h"


namespace re
{
	class Context;
}


namespace opengl
{
	class Context
	{
	public:
		struct PlatformParams : public virtual platform::Context::PlatformParams
		{
			PlatformParams() = default;
			~PlatformParams() override = default;

			SDL_GLContext m_glContext = 0;
		};

	public:
		static void Create(re::Context& context);
		static void Destroy(re::Context& context);
		static void SwapWindow(re::Context const& context);
		static void SetCullingMode(platform::Context::FaceCullingMode const& mode);
		static void ClearTargets(platform::Context::ClearTarget const& clearTarget);
		static void SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst);
		static void SetDepthTestMode(platform::Context::DepthTestMode const& mode);
		static void SetDepthWriteMode(platform::Context::DepthWriteMode const& mode);
		static void SetColorWriteMode(platform::Context::ColorWriteMode const& channelModes);
		static uint32_t GetMaxTextureInputs();
	};
}