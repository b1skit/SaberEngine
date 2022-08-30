#pragma once

#include "GraphicsSystem.h"
#include "LogManager.h"

namespace gr
{
	GraphicsSystem::GraphicsSystem(std::string name) :
		m_name(name)
	{
		LOG("Creating " + name);
	}
}