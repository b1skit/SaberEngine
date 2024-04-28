// © 2024 Adam Badke. All rights reserved.
#include "Core\Assert.h"
#include "Context.h"
#include "RenderManager.h"
#include "RLibrary_Platform.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"
#include "RenderStage.h"


namespace platform
{
	bool RLibrary::RegisterPlatformLibraries()
	{
		const platform::RenderingAPI& api = re::RenderManager::Get()->GetRenderingAPI();

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


	void RLibrary::Execute(re::RenderStage* renderStage)
	{
		SEAssert(renderStage->GetStageType() == re::RenderStage::Type::Library, "Invalid stage type");

		re::RenderStage::LibraryStageParams const* libraryStageParams =
			dynamic_cast<re::RenderStage::LibraryStageParams const*>(renderStage->GetStageParams());

		switch (libraryStageParams->m_type)
		{
		case re::RenderStage::LibraryStageParams::LibraryType::ImGui:
		{
			dynamic_cast<platform::RLibraryImGui*>(
				re::Context::Get()->GetOrCreateRenderLibrary(platform::RLibrary::Type::ImGui))->Execute(renderStage);
		}
		break;
		default: SEAssertF("Invalid library type");
		}
	}
}