// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"
#include "RenderStage.h"


namespace gr
{
	class GBufferGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		static const std::vector<std::string> GBufferTexNames;

	public:
		explicit GBufferGraphicsSystem(std::string name);

		GBufferGraphicsSystem() = delete;

		~GBufferGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_gBufferStage;

	};
}