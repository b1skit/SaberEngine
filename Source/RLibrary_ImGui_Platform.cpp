// © 2024 Adam Badke. All rights reserved.
#include "Core\Definitions\ConfigKeys.h"
#include "RenderManager.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"


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
		io.IniFilename = core::configkeys::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
	}


	// ---


	std::unique_ptr<RLibrary>(*RLibraryImGui::Create)() = nullptr;
}