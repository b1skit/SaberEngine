// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class ComputeMipsGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<ComputeMipsGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "ComputeMips"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(ComputeMipsGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(ComputeMipsGraphicsSystem, PreRender))
			);
		}

		void RegisterInputs() override {};
		void RegisterOutputs() override {};


	public:
		ComputeMipsGraphicsSystem(gr::GraphicsSystemManager*);

		~ComputeMipsGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline&, TextureDependencies const&);

		void PreRender(DataDependencies const&);


	private:
		re::StagePipeline::StagePipelineItr m_parentStageItr;
		re::StagePipeline* m_stagePipeline;
	};
}