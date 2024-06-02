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

		static constexpr util::HashKey k_tonemappingTargetInput = "TonemappingTarget";
		static constexpr util::HashKey k_bloomResultInput = "BloomResult";
		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		TonemappingGraphicsSystem(gr::GraphicsSystemManager*);

		~TonemappingGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&);
		void PreRender(DataDependencies const&);


	private:
		std::shared_ptr<re::RenderStage> m_tonemappingStage;
	};
}