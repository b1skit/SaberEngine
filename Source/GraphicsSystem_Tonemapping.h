// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class TonemappingGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<TonemappingGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Tonemapping"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(TonemappingGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(TonemappingGraphicsSystem, PreRender))
			);
		}

		static constexpr char const* k_tonemappingTargetInput = "TonemappingTarget";
		static constexpr char const* k_bloomResultInput = "BloomResult";
		void RegisterTextureInputs() override;

		void RegisterTextureOutputs() override;


	public:
		TonemappingGraphicsSystem(gr::GraphicsSystemManager*);

		~TonemappingGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline&, TextureDependencies const&);

		void PreRender();


	private:
		std::shared_ptr<re::RenderStage> m_tonemappingStage;
	};
}