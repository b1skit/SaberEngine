#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		BloomGraphicsSystem(std::string name);

		BloomGraphicsSystem() = delete;
		~BloomGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_emissiveBlitStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_emissiveBlitStage.GetTextureTargetSet(); }


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_screenAlignedQuad;

		gr::RenderStage m_emissiveBlitStage;
		std::vector<gr::RenderStage> m_downResStages;
		std::vector<gr::RenderStage> m_blurStages;
		std::vector<gr::RenderStage> m_upResStages;

		const uint32_t m_numDownSamplePasses	= 2; // Scaling factor: # times we half the frame size
		const uint32_t m_numBlurPasses			= 3; // How many pairs of horizontal + vertical blur passes to perform
	};
}