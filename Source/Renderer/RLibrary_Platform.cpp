// © 2024 Adam Badke. All rights reserved.
#include "Core/Assert.h"
#include "Context.h"
#include "RenderManager.h"
#include "RLibrary_Platform.h"
#include "RLibrary_ImGui_DX12.h"
#include "RLibrary_ImGui_OpenGL.h"
#include "RLibrary_ImGui_Platform.h"
#include "Stage.h"

#include "Core/Logger.h"


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


	void RLibrary::Execute(gr::Stage* stage, void* platformObject)
	{
		SEAssert(stage->GetStageType() == gr::Stage::Type::LibraryRaster ||
			stage->GetStageType() == gr::Stage::Type::LibraryCompute,
			"Invalid stage type");

		gr::Stage::LibraryStageParams const* libraryStageParams =
			dynamic_cast<gr::Stage::LibraryStageParams const*>(stage->GetStageParams());

		switch (libraryStageParams->m_type)
		{
		case gr::Stage::LibraryStageParams::LibraryType::ImGui:
		{
			dynamic_cast<platform::RLibraryImGui*>(
				re::RenderManager::Get()->GetContext()->GetOrCreateRenderLibrary(
					platform::RLibrary::Type::ImGui))->Execute(stage, platformObject);
		}
		break;
		default: SEAssertF("Invalid library type");
		}
	}
}