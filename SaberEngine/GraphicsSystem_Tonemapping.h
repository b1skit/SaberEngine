#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class TonemappingGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		TonemappingGraphicsSystem(std::string name);

		TonemappingGraphicsSystem() = delete;
		~TonemappingGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender() override;

		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_tonemappingStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_tonemappingStage.GetTextureTargetSet(); }


	private:
		std::vector<std::shared_ptr<gr::Mesh>> m_screenAlignedQuad;

		gr::RenderStage m_tonemappingStage;
	};
}