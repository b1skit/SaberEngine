// © 2024 Adam Badke. All rights reserved.
#include "Context.h"
#include "RenderManager.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Core/Host/Window_Win32.h"


namespace platform
{
	void RLibraryImGui::CreatePlatformParams(RLibraryImGui& imguiLibrary)
	{
		SEAssert(imguiLibrary.GetPlatformParams() == nullptr,
			"Attempting to create platform params for a buffer that already exists");

		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			imguiLibrary.SetPlatformParams(std::make_unique<opengl::RLibraryImGui::PlatformParams>());
		}
		break;
		case RenderingAPI::DX12:
		{
			imguiLibrary.SetPlatformParams(std::make_unique<dx12::RLibraryImGui::PlatformParams>());
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
		CreatePlatformParams(imguiLibrary);

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
		host::Window* window = re::Context::Get()->GetWindow();
		SEAssert(window, "Window pointer cannot be null");

		win32::Window::PlatformParams* windowPlatParams =
			window->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		ImGuiIO& io = ImGui::GetIO();

		const float fontPixelSize = 15 * windowPlatParams->m_windowScale;
		io.Fonts->AddFontFromFileTTF("Assets\\Fonts\\source-code-pro.regular.ttf", fontPixelSize);

		ImGui::GetStyle().ScaleAllSizes(windowPlatParams->m_windowScale);
	}


	// ---


	std::unique_ptr<RLibrary>(*RLibraryImGui::Create)() = nullptr;
}