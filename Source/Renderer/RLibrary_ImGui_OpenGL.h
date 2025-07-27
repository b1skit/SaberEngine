// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "RLibrary_ImGui_Platform.h"


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

		void Execute(std::unique_ptr<platform::RLibrary::IPayload>&&, void* platformObject) override;

		void Destroy() override;
	};
}