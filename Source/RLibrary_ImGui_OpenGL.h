// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_ImGui_Platform.h"


namespace re
{
	class RenderStage;
}

namespace opengl
{
	class RLibraryImGui final : public virtual platform::RLibraryImGui
	{
	public:
		struct PlatformParams : public platform::RLibraryImGui::PlatformParams
		{
			//
		};


	public:
		static std::unique_ptr<platform::RLibrary> Create();

	public:
		RLibraryImGui() = default;
		~RLibraryImGui() = default;

		void Execute(re::RenderStage*) override;

		void Destroy() override;


	private:

	};
}