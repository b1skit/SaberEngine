// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class SkyboxGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		SkyboxGraphicsSystem(gr::GraphicsSystemManager*);

		~SkyboxGraphicsSystem() override {}

		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


		void ShowImGuiWindow() override;

	private:
		void CreateBatches() override;


	private:
		std::shared_ptr<re::RenderStage> m_skyboxStage;
		re::Texture const* m_skyTexture;
		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::unique_ptr<re::Batch> m_fullscreenQuadBatch;
		std::shared_ptr<re::ParameterBlock> m_skyboxParams;
		
		glm::vec3 m_backgroundColor;
		bool m_showBackgroundColor;
		bool m_isDirty;
	};
}