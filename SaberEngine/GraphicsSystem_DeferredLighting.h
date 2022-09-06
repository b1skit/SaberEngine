#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class DeferredLightingGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		DeferredLightingGraphicsSystem(std::string name);

		DeferredLightingGraphicsSystem() = delete;
		~DeferredLightingGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender() override;

		// Note: All light stages write to the same target
		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_ambientStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_ambientStage.GetTextureTargetSet(); }

	private:
		gr::RenderStage m_ambientStage;;
		std::vector<std::shared_ptr<gr::Mesh>> m_ambientMesh;

		gr::RenderStage m_keylightStage;
		std::vector<std::shared_ptr<gr::Mesh>> m_keylightMesh;

		gr::RenderStage m_pointlightStage;
		std::vector<std::shared_ptr<gr::Mesh>> m_pointlightInstanceMesh;

	};
}