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
	class XeGTAOGraphicsSystem;


	class DeferredLightingGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		DeferredLightingGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredLightingGraphicsSystem() override = default;

		void CreateResourceGenerationStages(re::StagePipeline&);
		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		// Note: All light stages write to the same target
		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		// Ambient IBL resources:
		std::shared_ptr<re::Texture> m_BRDF_integrationMap;
		std::shared_ptr<re::Texture> m_IEMTex;
		std::shared_ptr<re::Texture> m_PMREMTex;
		std::shared_ptr<re::RenderStage> m_ambientStage;
		std::shared_ptr<re::ParameterBlock> m_ambientParams;
		XeGTAOGraphicsSystem* m_AOGS;

		// TODO: We only use this in the 1st frame, we should clean it up via a virtual "end frame cleanup" GS function
		std::shared_ptr<gr::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. 

		// Punctual lights:
		struct PunctualLightData
		{
			gr::Light::Type m_type;
			std::shared_ptr<re::ParameterBlock> m_lightParams;
			std::shared_ptr<re::ParameterBlock> m_transformParams;
			re::Batch m_batch;
		};
		std::unordered_map<gr::RenderDataID, PunctualLightData> m_lightData;

		std::shared_ptr<re::RenderStage> m_directionalStage;
		std::shared_ptr<re::RenderStage> m_pointStage;

		gr::ShadowsGraphicsSystem* m_shadowGS;


	private:
		void CreateBatches() override;
	};


	inline std::shared_ptr<re::TextureTargetSet const> DeferredLightingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_ambientStage->GetTextureTargetSet();
	}
}