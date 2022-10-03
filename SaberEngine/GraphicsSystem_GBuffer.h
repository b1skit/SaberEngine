#pragma once

#include "GraphicsSystem.h"
#include "RenderStage.h"


namespace gr
{
	class GBufferGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		static const std::vector<std::string> GBufferTexNames;

	public:
		explicit GBufferGraphicsSystem(std::string name);

		GBufferGraphicsSystem() = delete;

		~GBufferGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_gBufferStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_gBufferStage.GetTextureTargetSet(); }

	private:
		gr::RenderStage m_gBufferStage;

	};
}