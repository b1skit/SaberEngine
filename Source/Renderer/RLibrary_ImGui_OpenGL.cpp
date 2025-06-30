// © 2024 Adam Badke. All rights reserved.
#include "Context.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RenderManager.h"
#include "Stage.h"

#include "Core/Host/Window_Win32.h"

#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"


namespace opengl
{
	std::unique_ptr<platform::RLibrary> RLibraryImGui::Create()
	{
		std::unique_ptr<platform::RLibrary> newLibrary = std::make_unique<opengl::RLibraryImGui>();

		platform::RLibraryImGui* imguiLibrary = dynamic_cast<platform::RLibraryImGui*>(newLibrary.get());
		platform::RLibraryImGui::CreateInternal(*imguiLibrary);

		// Setup OpenGL ImGui backend:
		host::Window* window = re::RenderManager::Get()->GetContext()->GetWindow();
		SEAssert(window, "Window pointer cannot be null");

		win32::Window::PlatObj* windowPlatObj =
			window->GetPlatformObject()->As<win32::Window::PlatObj*>();

		::ImGui_ImplWin32_Init(windowPlatObj->m_hWindow);
		::ImGui_ImplWin32_EnableDpiAwareness();

		constexpr char const* imguiGLSLVersionString = "#version 130";
		::ImGui_ImplOpenGL3_Init(imguiGLSLVersionString);

		platform::RLibraryImGui::ConfigureScaling(*dynamic_cast<platform::RLibraryImGui*>(newLibrary.get()));

		return std::move(newLibrary);
	}


	void RLibraryImGui::Destroy()
	{
		LOG("Destroying ImGui render library");

		// Imgui cleanup
		::ImGui_ImplOpenGL3_Shutdown();
		::ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}


	void RLibraryImGui::Execute(gr::Stage* stage, void* platformObject)
	{
		gr::LibraryStage* imGuiStage = dynamic_cast<gr::LibraryStage*>(stage);

		std::unique_ptr<gr::LibraryStage::IPayload> iPayload = imGuiStage->TakePayload();
		platform::RLibraryImGui::Payload* payload = dynamic_cast<platform::RLibraryImGui::Payload*>(iPayload.get());

		RLibraryImGui* imGuiLibrary = dynamic_cast<RLibraryImGui*>(
			re::RenderManager::Get()->GetContext()->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		SEAssert(imGuiStage && payload && imGuiLibrary, "A critical resource is null");

		if (payload->m_perFrameCommands->HasCommandsToExecute(payload->m_currentFrameNum))
		{
			// Start the ImGui frame:
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// Execute our queued commands:
			payload->m_perFrameCommands->Execute(payload->m_currentFrameNum);

			// Composite Imgui rendering on top of the finished frame:
			SEBeginOpenGLGPUEvent(perfmarkers::Type::GraphicsCommandList, "ImGui stage");
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			SEEndOpenGLGPUEvent();
		}
	}
}
