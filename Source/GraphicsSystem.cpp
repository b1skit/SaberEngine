// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem.h"
#include "GraphicsSystemManager.h"
#include "LogManager.h"
#include "RenderSystem.h"


namespace gr
{
	GraphicsSystem::GraphicsSystem(
		std::string const& name, gr::GraphicsSystemManager* owningGSM)
		: NamedObject(name)
		, m_owningGraphicsSystemManager(owningGSM)
	{
		LOG("Creating %s", name.c_str());
	}


	void GraphicsSystem::ShowImGuiWindow()
	{
		ImGui::Text("...");
	}
}