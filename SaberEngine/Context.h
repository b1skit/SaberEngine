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

		std::unique_ptr<platform::Context::PlatformParams>& GetPlatformParams() { return m_platformParams; }

		// Platform wrappers:
		void Create();
		void Destroy();
		void SwapWindow();


	private:
		std::unique_ptr<platform::Context::PlatformParams> m_platformParams;

		// Friends:
		friend void platform::Context::PlatformParams::CreatePlatformParams(re::Context& context);
	};
}