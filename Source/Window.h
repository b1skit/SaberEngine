// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class Window
	{
	public:
		struct PlatformParams
		{
			PlatformParams() = default;
			PlatformParams(PlatformParams const&) = delete;
			virtual ~PlatformParams() = 0;
		};


	public:
		Window();
		
		~Window() { Destroy(); };

		void SetFocusState(bool hasFocus); // To be called by event handlers only
		bool GetFocusState() const;

		Window::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Window::PlatformParams> params) { m_platformParams = std::move(params); }


		// Platform wrappers:
		bool Create(std::string const& title, uint32_t width, uint32_t height);
		void Destroy();
		void SetRelativeMouseMode(bool enabled) const; // Hides cursor and wraps movements around boundaries


	private:
		std::unique_ptr<Window::PlatformParams> m_platformParams;
		bool m_hasFocus;

	private:
		// Copying not allowed
		Window(Window const&) = delete;
		Window(Window&&) = delete;
		Window& operator=(Window const&) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Window::PlatformParams::~PlatformParams() {};
}
