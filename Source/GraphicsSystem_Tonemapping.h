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


	public:
		TonemappingGraphicsSystem(gr::GraphicsSystemManager*);

		~TonemappingGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;

		std::shared_ptr<re::RenderStage> m_tonemappingStage;
	};


	inline std::shared_ptr<re::TextureTargetSet const> TonemappingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_tonemappingStage->GetTextureTargetSet();
	}
}