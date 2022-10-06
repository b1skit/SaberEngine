#include "GraphicsSystem.h"
#include "LogManager.h"

namespace gr
{
	GraphicsSystem::GraphicsSystem(std::string const& name) : NamedObject(name)
	{
		LOG("Creating %s", name.c_str());
	}
}