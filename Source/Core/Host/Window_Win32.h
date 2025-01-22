// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Window.h"

#include "../Assert.h"
#include "../LogManager.h" // TODO: Remove this

#include "oleidl.h"


namespace win32
{
	class SEWindowDropTarget;


	class Window
	{
	public:
		struct Win32PlatformState
		{
			HINSTANCE m_hInstance = NULL;

			// Mouse cursors:
			HCURSOR m_defaultCursor{}; // Default class cursor
		};
		static Win32PlatformState s_platformState;


	public:
		struct PlatformParams final : public host::Window::PlatformParams
		{
			HWND m_hWindow = NULL;

			std::unique_ptr<SEWindowDropTarget> m_dropTarget;

			bool m_OLEIInitialized = false;

			float m_windowScale = 0.f; // e.g. Windows Settings -> Display -> Scale & layout
		};


	public:
		static LRESULT CALLBACK WindowEventCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);


	public:
		static bool Create(host::Window&, host::Window::CreateParams const&);
		static void Destroy(host::Window&);

		static void SetRelativeMouseMode(host::Window const&, bool enabled);
	};


	class SEWindowDropTarget final : public virtual IDropTarget
	{
	public:
		~SEWindowDropTarget();


	public: // IUnknown overrides:		
		HRESULT QueryInterface(REFIID riid, void** ppv) override;
		ULONG AddRef() override;
		ULONG Release() override;


	public: // IDropTarget overrides:
		HRESULT DragEnter(IDataObject*, DWORD, POINTL, DWORD*) override;
		HRESULT DragOver(DWORD, POINTL,DWORD*) override;
		HRESULT DragLeave() override;
		HRESULT Drop(IDataObject*, DWORD, POINTL, DWORD*) override;


	private:
		std::atomic<ULONG> m_refCount; // For posterity: We don't currently need this as our window manages the lifetime
	};
}