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

		~DeferredLightingGraphicsSystem() override {}

		void CreateResourceGenerationStages(re::StagePipeline&);
		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		// Note: All light stages write to the same target
		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		std::shared_ptr<re::RenderStage> m_ambientStage;

		std::shared_ptr<re::MeshPrimitive> m_screenAlignedQuad;
		std::shared_ptr<re::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. TODO: We only use this in the 1st frame, should probably clean it up
		std::vector<std::shared_ptr<gr::Mesh>> m_sphereMeshes; // Deferred point lights

		std::shared_ptr<re::RenderStage> m_keylightStage;

		std::shared_ptr<re::RenderStage> m_pointlightStage;
		std::vector<std::shared_ptr<re::MeshPrimitive>> m_pointlightInstanceMesh;


	private:
		void CreateBatches() override;
	};
}