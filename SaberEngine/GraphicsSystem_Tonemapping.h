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

		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_tonemappingStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_tonemappingStage.GetTextureTargetSet(); }

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<gr::Mesh> m_screenAlignedQuad;
		gr::RenderStage m_tonemappingStage;		
	};
}