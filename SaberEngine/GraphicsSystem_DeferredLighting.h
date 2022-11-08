#pragma once

#include "GraphicsSystem.h"


namespace gr
{
	class DeferredLightingGraphicsSystem : public virtual GraphicsSystem
	{
	public:
		explicit DeferredLightingGraphicsSystem(std::string name);

		DeferredLightingGraphicsSystem() = delete;
		~DeferredLightingGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		// Note: All light stages write to the same target
		gr::TextureTargetSet& GetFinalTextureTargetSet() override { return m_ambientStage.GetTextureTargetSet(); }
		gr::TextureTargetSet const& GetFinalTextureTargetSet() const override { return m_ambientStage.GetTextureTargetSet(); }


	private:
		gr::RenderStage m_ambientStage;

		std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
		std::shared_ptr<gr::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. TODO: We only use this in the 1st frame, should probably clean it up

		std::shared_ptr<gr::Texture> m_BRDF_integrationMap;
		std::shared_ptr<gr::Texture> m_IEMTex;
		std::shared_ptr<gr::Texture> m_PMREMTex;

		gr::RenderStage m_keylightStage;

		gr::RenderStage m_pointlightStage;
		std::vector<std::shared_ptr<gr::MeshPrimitive>> m_pointlightInstanceMesh;


	private:
		void CreateBatches() override;

		inline bool AmbientIsValid() const { return m_BRDF_integrationMap && m_IEMTex && m_PMREMTex && m_screenAlignedQuad; }
	};
}