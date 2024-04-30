// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core\Interfaces\IPlatformParams.h"


namespace app
{
	class Window
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
		};


	public:
		Window();
		Window(Window&&) = default;
		Window& operator=(Window&&) = default;
		~Window() { Destroy(); };

		Window::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Window::PlatformParams> params) { m_platformParams = std::move(params); }

		// Platform wrappers:
		bool Create(std::string const& title, uint32_t width, uint32_t height);
		void Destroy();

		void SetFocusState(bool hasFocus); // To be called by event handlers only
		bool GetFocusState() const;

		void SetRelativeMouseMode(bool enabled); // Hides cursor and wraps movements around boundaries


	private:
		std::unique_ptr<Window::PlatformParams> m_platformParams;
		bool m_hasFocus;
		bool m_relativeMouseModeEnabled;


	private: // Copying not allowed
		Window(Window const&) = delete;
		Window& operator=(Window const&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Window::PlatformParams::~PlatformParams() {};
}
