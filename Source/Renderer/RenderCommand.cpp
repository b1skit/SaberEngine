// © 2025 Adam Badke. All rights reserved.
#include "RenderCommand.h"


namespace gr
{
	core::CommandManager* RenderCommand::s_renderCommandManager = nullptr;
	gr::RenderDataManager* RenderCommand::s_renderDataManager = nullptr;
}