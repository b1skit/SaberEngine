// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Private/RLibrary_ImGui_Platform.h"


namespace re
{
	class Stage;
}

namespace opengl
{
	class RLibraryImGui final : public virtual platform::RLibraryImGui
	{
	public:
		struct PlatObj : public platform::RLibraryImGui::PlatObj
		{
			//
		};


	public:
		static std::unique_ptr<platform::RLibrary> Create();

	public:
		RLibraryImGui() = default;
		~RLibraryImGui() = default;

		void Execute(re::Stage*, void* platformObject) override;

		void Destroy() override;


	private:

	};
}