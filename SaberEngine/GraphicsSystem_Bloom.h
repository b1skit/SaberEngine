#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		explicit BloomGraphicsSystem(std::string name);

		BloomGraphicsSystem() = delete;
		~BloomGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		re::TextureTargetSet& GetFinalTextureTargetSet() override { return m_emissiveBlitStage.GetTextureTargetSet(); }
		re::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_emissiveBlitStage.GetTextureTargetSet(); }

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;

		re::RenderStage m_emissiveBlitStage;
		std::vector<re::RenderStage> m_downResStages;
		std::vector<re::RenderStage> m_blurStages;
		std::vector<re::RenderStage> m_upResStages;

		const uint32_t m_numDownSamplePasses	= 2; // Scaling factor: # times we half the frame size
		const uint32_t m_numBlurPasses			= 3; // How many pairs of horizontal + vertical blur passes to perform
	};
}