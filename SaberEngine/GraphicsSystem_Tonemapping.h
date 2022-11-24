#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class TonemappingGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		explicit TonemappingGraphicsSystem(std::string name);

		TonemappingGraphicsSystem() = delete;
		~TonemappingGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		re::TextureTargetSet& GetFinalTextureTargetSet() override { return m_tonemappingStage.GetTextureTargetSet(); }
		re::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_tonemappingStage.GetTextureTargetSet(); }

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
		re::RenderStage m_tonemappingStage;		
	};
}