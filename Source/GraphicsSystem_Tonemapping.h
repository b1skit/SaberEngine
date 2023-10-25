// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class TonemappingGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		TonemappingGraphicsSystem();

		~TonemappingGraphicsSystem() override {}

		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
		std::shared_ptr<re::RenderStage> m_tonemappingStage;

		re::RenderSystem* m_owningRenderSystem;
	};
}