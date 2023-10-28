// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class SkyboxGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		SkyboxGraphicsSystem();

		~SkyboxGraphicsSystem() override {}

		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;


	private:
		std::shared_ptr<re::RenderStage> m_skyboxStage;
		std::shared_ptr<re::Texture> m_skyTexture;
		std::string m_skyTextureShaderName;
		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
	};
}