// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"
#include "RenderStage.h"


namespace gr
{
	class GBufferGraphicsSystem final : public virtual GraphicsSystem
	{
	public:
		// These enums are converted to strings, & must align with the layout binding indexes defined in SaberCommon.glsl
		enum GBufferTexIdx
		{
			GBufferAlbedo	= 0,
			GBufferWNormal	= 1,
			GBufferRMAO		= 2,
			GBufferEmissive = 3,
			GBufferWPos		= 4,
			GBufferMatProp0 = 5,
			GBufferDepth	= 6,

			GBufferTexIdx_Count
		};
		static const std::array<std::string, GBufferTexIdx_Count> GBufferTexNames;


	public:
		explicit GBufferGraphicsSystem(std::string name);

		GBufferGraphicsSystem() = delete;

		~GBufferGraphicsSystem() override {}

		void Create(re::StagePipeline& pipeline) override;

		void PreRender(re::StagePipeline& pipeline) override;

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;

	private:
		re::RenderStage m_gBufferStage;

	};
}