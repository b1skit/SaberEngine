// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"

namespace gr
{
	class Mesh;
}
namespace re
{
	class MeshPrimitive;
	class Texture;
}


namespace gr
{
	class DeferredLightingGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		explicit DeferredLightingGraphicsSystem(std::string name);

		DeferredLightingGraphicsSystem() = delete;
		~DeferredLightingGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		// Note: All light stages write to the same target
		std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const override;


	private:
		re::RenderStage m_ambientStage;

		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
		std::shared_ptr<re::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. TODO: We only use this in the 1st frame, should probably clean it up
		std::vector<std::shared_ptr<gr::Mesh>> m_sphereMeshes; // Deferred point lights

		std::shared_ptr<re::Texture> m_BRDF_integrationMap;
		std::shared_ptr<re::Texture> m_IEMTex;
		std::shared_ptr<re::Texture> m_PMREMTex;

		re::RenderStage m_keylightStage;

		re::RenderStage m_pointlightStage;
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_pointlightInstanceMesh;


	private:
		void CreateBatches() override;

		inline bool AmbientIsValid() const { return m_BRDF_integrationMap && m_IEMTex && m_PMREMTex && m_screenAlignedQuad; }
	};
}