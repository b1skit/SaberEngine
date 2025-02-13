// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_AccelerationStructures.h"
#include "GraphicsSystemManager.h"



namespace gr
{
	AccelerationStructuresGraphicsSystem::AccelerationStructuresGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void AccelerationStructuresGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		//
	}


	void AccelerationStructuresGraphicsSystem::RegisterInputs()
	{
		//
	}


	void AccelerationStructuresGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void AccelerationStructuresGraphicsSystem::PreRender()
	{
		//
	}


	//void AccelerationStructuresGraphicsSystem::ShowImGuiWindow()
	//{
	//	//
	//}
}