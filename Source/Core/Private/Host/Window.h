// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Private/../Interfaces/IPlatformObject.h"


namespace fr
{
	class UIManager;
}
namespace win32
{
	class Window;
}

namespace host
{
	class Window
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			virtual ~PlatObj() = default;
		};


		struct CreateParams
		{
			std::string m_title;
			uint32_t m_width;
			uint32_t m_height;
			bool m_allowDragAndDrop = false;
		};


	public:
		Window();
		Window(Window&&) noexcept = default;
		Window& operator=(Window&&) noexcept = default;
		~Window();

		Window::PlatObj* GetPlatformObject() const { return m_platObj.get(); }
		void SetPlatformObject(std::unique_ptr<Window::PlatObj> platObj) { m_platObj = std::move(platObj); }

		// Platform wrappers:
		bool Create(CreateParams const&); // Must be called from the thread that owns the OS event queue
		void Destroy();


	protected:
		friend class win32::Window;
		void SetFocusState(bool hasFocus); // To be called by event handlers only


	protected:
		friend class fr::UIManager;
		void SetRelativeMouseMode(bool enabled); // enabled: Hides cursor and wraps movements around boundaries


	private:
		std::unique_ptr<Window::PlatObj> m_platObj;
		bool m_hasFocus;
		bool m_relativeMouseModeEnabled;


	private: // Copying not allowed
		Window(Window const&) = delete;
		Window& operator=(Window const&) = delete;
	};
}
