// � 2023 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class TempDebugGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		explicit TempDebugGraphicsSystem(std::string name);

		TempDebugGraphicsSystem() = delete;
		~TempDebugGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_tempDebugStage;

		std::shared_ptr<re::MeshPrimitive> m_helloTriangle;
	};
}