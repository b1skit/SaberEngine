// © 2024 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Context.h"
#include "RLibrary_Platform.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"

#include "Core/Config.h"
#include "Core/Logger.h"


namespace platform
{
	bool RLibrary::RegisterPlatformLibraries()
	{
		const platform::RenderingAPI api =
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);

		bool result = true;

		switch (api)
		{
		case RenderingAPI::OpenGL:
		{
			platform::RLibraryImGui::Create = opengl::RLibraryImGui::Create;
		}
		break;
		case RenderingAPI::DX12:
		{
			platform::RLibraryImGui::Create = dx12::RLibraryImGui::Create;
		}
		break;
		{
			SEAssertF("Unsupported rendering API");
			result = false;
		}
		}
		return result;
	}


	// ---


	std::unique_ptr<RLibrary> RLibrary::Create(Type type)
	{
		switch (type)
		{
		case Type::ImGui:
		{
			LOG("Creating ImGui render library");
			return platform::RLibraryImGui::Create();
		}
		break;
		default: SEAssertF("Invalid type");
		}
		return nullptr;
	}


	void RLibrary::Execute(
		re::Context* context, Type libraryType, std::unique_ptr<IPayload>&& iPayload, void* platformObject)
	{
		switch (libraryType)
		{
		case platform::RLibrary::Type::ImGui:
		{
			dynamic_cast<platform::RLibraryImGui*>(
				context->GetOrCreateRenderLibrary(platform::RLibrary::Type::ImGui))->Execute(
					std::forward<std::unique_ptr<IPayload>>(iPayload),
					platformObject);
		}
		break;
		default: SEAssertF("Invalid library type");
		}
	}
}