// Š 2024 Adam Badke. All rights reserved.
#include "Context.h"
#include "RenderManager.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"

#include "Core/Config.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Host/Window_Win32.h"


namespace platform
{
	void RLibraryImGui::CreatePlatformObject(RLibraryImGui& imguiLibrary)
	{
		SEAssert(imguiLibrary.GetPlatformObject() == nullptr,
			"Attempting to create platform object for a buffer that already exists");

		const platform::RenderingAPI api =
			core::Config::Get()->GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			imguiLibrary.SetPlatformObject(std::make_unique<opengl::RLibraryImGui::PlatObj>());
		}
		break;
		case RenderingAPI::DX12:
		{
			imguiLibrary.SetPlatformObject(std::make_unique<dx12::RLibraryImGui::PlatObj>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void RLibraryImGui::CreateInternal(RLibraryImGui& imguiLibrary)
	{
		CreatePlatformObject(imguiLibrary);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		// Configure ImGui:
		io.IniFilename = core::configkeys::k_imguiIniPath;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
	}


	void RLibraryImGui::ConfigureScaling(RLibraryImGui& imguiLibrary)
	{
		host::Window* window = gr::RenderManager::Get()->GetContext()->GetWindow();
		SEAssert(window, "Window pointer cannot be null");

		win32::Window::PlatObj* windowPlatObj =
			window->GetPlatformObject()->As<win32::Window::PlatObj*>();

		ImGuiIO& io = ImGui::GetIO();

		const float fontPixelSize = 15 * windowPlatObj->m_windowScale;
		io.Fonts->AddFontFromFileTTF("Assets\\Fonts\\source-code-pro.regular.ttf", fontPixelSize);

		ImGui::GetStyle().ScaleAllSizes(windowPlatObj->m_windowScale);
	}


	// ---


	std::unique_ptr<RLibrary>(*RLibraryImGui::Create)() = nullptr;
}