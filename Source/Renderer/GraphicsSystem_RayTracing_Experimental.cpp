// © 2025 Adam Badke. All rights reserved.
#include "GraphicsSystem_RayTracing_Experimental.h"


namespace gr
{
	RayTracing_ExperimentalGraphicsSystem::RayTracing_ExperimentalGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void RayTracing_ExperimentalGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_sceneTLASInput);
	}


	void RayTracing_ExperimentalGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void RayTracing_ExperimentalGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const&, 
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		m_sceneTLAS = GetDataDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		SEAssert(m_sceneTLAS, "Scene TLAS cannot be null");
	}


	void RayTracing_ExperimentalGraphicsSystem::PreRender()
	{
		//
	}
}