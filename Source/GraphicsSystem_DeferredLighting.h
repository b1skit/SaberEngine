// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"
#include "LightRenderData.h"

namespace re
{
	class MeshPrimitive;
}


namespace gr
{
	class ShadowsGraphicsSystem;


	class DeferredLightingGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		DeferredLightingGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredLightingGraphicsSystem() override;

		void CreateResourceGenerationStages(re::StagePipeline&);
		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		// Note: All light stages write to the same target
		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;

		void RegisterLight(gr::Light::Type, gr::RenderDataID);
		void UnregisterLight(gr::Light::Type, gr::RenderDataID);


	private:
		std::array<std::vector<gr::RenderDataID>, gr::Light::Type_Count> m_lightRenderDataIDs;

		gr::ShadowsGraphicsSystem* m_shadowGS;

		// Ambient IBL resources:
		std::shared_ptr<re::Texture> m_BRDF_integrationMap;
		std::shared_ptr<re::Texture> m_IEMTex;
		std::shared_ptr<re::Texture> m_PMREMTex;
		std::shared_ptr<re::RenderStage> m_ambientStage;
		std::shared_ptr<re::ParameterBlock> m_ambientParams;

		// TODO: We only use this in the 1st frame, we should clean it up via a virtual "end frame cleanup" GS function
		std::shared_ptr<gr::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. 

		std::shared_ptr<re::RenderStage> m_directionalStage;

		std::shared_ptr<re::RenderStage> m_pointStage;


	private:
		void CreateBatches() override;
	};


	inline std::shared_ptr<re::TextureTargetSet const> DeferredLightingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_ambientStage->GetTextureTargetSet();
	}
}