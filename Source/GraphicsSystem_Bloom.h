// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class BloomGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		BloomGraphicsSystem(gr::GraphicsSystemManager*);

		~BloomGraphicsSystem() override {}

		void Create(re::RenderSystem&, re::StagePipeline& pipeline);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;
	

	private:
		void CreateBatches() override;


	private:
		re::RenderSystem* m_owningRenderSystem;

		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;

		// Emissive blit:
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