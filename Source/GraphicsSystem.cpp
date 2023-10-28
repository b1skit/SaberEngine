// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem.h"
#include "LogManager.h"

namespace gr
{
	GraphicsSystem::GraphicsSystem(std::string const& name)
		: NamedObject(name)
	{
		LOG("Creating %s", name.c_str());
	}


	void GraphicsSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(GetName().c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("...");
		}
	}
}