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

		std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
		re::RenderStage m_tonemappingStage;		
	};
}