#pragma once

#include <memory>

#include "GraphicsSystem.h"


namespace gr
{
	class SkyboxGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		SkyboxGraphicsSystem(std::string name);

		SkyboxGraphicsSystem() = delete;
		~SkyboxGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender() override;

		gr::TextureTargetSet& GetFinalTextureTargetSet() override {	return m_skyboxStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override {	return m_skyboxStage.GetTextureTargetSet(); }


	private:
		gr::RenderStage m_skyboxStage;
		std::shared_ptr<gr::Texture> m_skyTexture;
		std::string m_skyTextureShaderName;
		std::vector<std::shared_ptr<gr::Mesh>> m_skyMesh;
	};
}