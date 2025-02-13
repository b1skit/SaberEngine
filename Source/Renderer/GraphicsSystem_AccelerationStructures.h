// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class AccelerationStructuresGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<AccelerationStructuresGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "AccelerationStructures"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(AccelerationStructuresGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(AccelerationStructuresGraphicsSystem, PreRender))
			);
		}


		void RegisterInputs() override;
		void RegisterOutputs() override;


	public:
		AccelerationStructuresGraphicsSystem(gr::GraphicsSystemManager*);
		~AccelerationStructuresGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();

		//void ShowImGuiWindow() override;


	private:

	};
}