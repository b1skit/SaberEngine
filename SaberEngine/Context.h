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

		// Platform wrappers:
		void Create();
		void Destroy();
		void SwapWindow();

		void SetCullingMode(platform::Context::FaceCullingMode const& mode);


	private:
		std::unique_ptr<platform::Context::PlatformParams> m_platformParams;

		// Friends:
		friend void platform::Context::PlatformParams::CreatePlatformParams(re::Context& context);
	};
}