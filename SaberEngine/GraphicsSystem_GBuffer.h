#pragma once

#include "GraphicsSystem.h"
#include "RenderStage.h"


namespace gr
{
	class GBufferGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		GBufferGraphicsSystem(std::string name);

		GBufferGraphicsSystem() = delete;

		~GBufferGraphicsSystem() override {}

		void Create(gr::RenderPipeline& pipeline) override;

		void PreRender() override;

		gr::TextureTargetSet& GetTextureTargetSet() override { return m_gBufferStage.GetStageTargetSet(); }
		gr::TextureTargetSet const& GetTextureTargetSet() const override { return m_gBufferStage.GetStageTargetSet(); }

	private:
		gr::RenderStage m_gBufferStage;

	};
}