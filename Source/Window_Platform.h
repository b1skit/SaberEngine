// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	class Window;
}

namespace platform
{
	class Window
	{
	public:
		struct PlatformParams
		{
			PlatformParams() = default;
			PlatformParams(PlatformParams const&) = delete;
			virtual ~PlatformParams() = 0;

			// API-specific function pointers:
			static void CreatePlatformParams(re::Window&);
		};


	public:
		static bool (*Create)(re::Window& window, std::string const& title, uint32_t width, uint32_t height);
		static void (*Destroy)(re::Window& window);
		static void (*SetRelativeMouseMode)(re::Window const& window, bool enabled);
	};

	// We need to provide a destructor implementation since it's pure virtual
	inline Window::PlatformParams::~PlatformParams() {};
}