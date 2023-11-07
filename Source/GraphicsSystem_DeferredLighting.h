// � 2022 Adam Badke. All rights reserved.
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
		DeferredLightingGraphicsSystem();

		~DeferredLightingGraphicsSystem() override {}

		void CreateResourceGenerationStages(re::StagePipeline&);
		void Create(re::RenderSystem&, re::StagePipeline&);

		void PreRender();

		// Note: All light stages write to the same target
		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		std::shared_ptr<re::RenderStage> m_ambientStage;
		std::shared_ptr<re::ParameterBlock> m_ambientParams;

		std::shared_ptr<gr::MeshPrimitive> m_cubeMeshPrimitive; // For rendering into a cube map. TODO: We only use this in the 1st frame, should probably clean it up

		std::shared_ptr<re::RenderStage> m_keylightStage;

		std::shared_ptr<re::RenderStage> m_pointlightStage;


	private:
		void CreateBatches() override;
	};
}