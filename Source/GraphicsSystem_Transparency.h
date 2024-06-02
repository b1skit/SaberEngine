// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class TransparencyGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<TransparencyGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Transparency"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(TransparencyGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(TransparencyGraphicsSystem, PreRender))
			);
		}

		static constexpr util::HashKey k_sceneDepthTexInput = "SceneDepth";
		static constexpr util::HashKey k_sceneLightingTexInput = "SceneLighting";
		static constexpr util::HashKey k_cullingDataInput = "ViewCullingResults";
		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		TransparencyGraphicsSystem(gr::GraphicsSystemManager*);

		~TransparencyGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&);

		void PreRender(DataDependencies const&);


	private:
		std::shared_ptr<re::RenderStage> m_transparencyStage;
	};
}