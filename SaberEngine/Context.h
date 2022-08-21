#pragma once

#include <memory>

#include "Context_Platform.h"


namespace re
{
	class Context
	{
	public:
		


	public:
		Context();

		platform::Context::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::Context::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

		// Platform wrappers:
		void Create();
		void Destroy();

		void SwapWindow() const;
		void SetCullingMode(platform::Context::FaceCullingMode const& mode) const;
		void ClearTargets(platform::Context::ClearTarget const& clearTarget) const;
		void SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst) const;


	private:
		std::unique_ptr<platform::Context::PlatformParams> m_platformParams;

		// Friends:
		friend void platform::Context::PlatformParams::CreatePlatformParams(re::Context& context);
	};
}