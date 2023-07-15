// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		explicit BloomGraphicsSystem(std::string name);

		BloomGraphicsSystem() = delete;
		~BloomGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;

		std::shared_ptr<re::RenderStage> m_emissiveBlitStage;
		std::vector<std::shared_ptr<re::RenderStage>> m_downResStages;
		std::vector<std::shared_ptr<re::RenderStage>> m_blurStages;
		std::vector<std::shared_ptr<re::RenderStage>> m_upResStages;

		const uint32_t m_numDownSamplePasses	= 2; // Scaling factor: # times we half the frame size
		const uint32_t m_numBlurPasses			= 3; // How many pairs of horizontal + vertical blur passes to perform
	};


	inline std::shared_ptr<re::TextureTargetSet const> BloomGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_emissiveBlitStage->GetTextureTargetSet();
	}
}