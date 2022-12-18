#pragma once

#include <memory>

#include "GraphicsSystem.h"


namespace gr
{
	class SkyboxGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		explicit SkyboxGraphicsSystem(std::string name);

		SkyboxGraphicsSystem() = delete;
		~SkyboxGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const override;

	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_skyboxStage;
		std::shared_ptr<re::Texture> m_skyTexture;
		std::string m_skyTextureShaderName;
		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
	};
}