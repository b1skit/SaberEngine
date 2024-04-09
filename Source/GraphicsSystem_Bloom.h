// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<BloomGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Bloom"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(BloomGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(BloomGraphicsSystem, PreRender))
			);
		}


	public:
		BloomGraphicsSystem(gr::GraphicsSystemManager*);

		~BloomGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline& pipeline);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;
	

	private:
		void CreateBatches() override;


	private:
		std::shared_ptr<re::RenderStage> m_emissiveBlitStage;

		// Bloom compute:
		std::vector<std::shared_ptr<re::RenderStage>> m_bloomDownStages;
		std::vector<std::shared_ptr<re::Buffer>> m_bloomDownBuffers;

		std::vector<std::shared_ptr<re::RenderStage>> m_bloomUpStages;
		std::vector<std::shared_ptr<re::Buffer>> m_bloomUpBuffers;

		std::shared_ptr<re::Shader> m_bloomComputeShader;

		uint32_t m_firstUpsampleSrcMipLevel; // == # of upsample stages
	};


	inline std::shared_ptr<re::TextureTargetSet const> BloomGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_bloomUpStages.back()->GetTextureTargetSet();
	}
}