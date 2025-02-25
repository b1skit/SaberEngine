// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace re
{
	class AccelerationStructure;
}

namespace gr
{
	class RayTracing_ExperimentalGraphicsSystem final
		: public virtual GraphicsSystem 
		, public virtual IScriptableGraphicsSystem<RayTracing_ExperimentalGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "RayTracing_Experimental"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(RayTracing_ExperimentalGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(RayTracing_ExperimentalGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_sceneTLASInput = "SceneTLAS";
		void RegisterInputs() override;
		void RegisterOutputs() override;


	public:
		RayTracing_ExperimentalGraphicsSystem(gr::GraphicsSystemManager*);
		~RayTracing_ExperimentalGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	private:
		std::shared_ptr<re::AccelerationStructure> const* m_sceneTLAS;
	};
}