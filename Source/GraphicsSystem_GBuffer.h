// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "GraphicsSystem.h"
#include "RenderStage.h"


namespace gr
{
	class GBufferGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<GBufferGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "GBuffer"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(GBufferGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(GBufferGraphicsSystem, PreRender))
			);
		}


	public:
		// These enums are converted to strings, & must align with the layout binding indexes defined in SaberCommon.glsl
		enum GBufferTexIdx : uint8_t
		{
			GBufferAlbedo	= 0,
			GBufferWNormal	= 1,
			GBufferRMAO		= 2,
			GBufferEmissive = 3,
			GBufferMatProp0 = 4,
			GBufferDepth	= 5,

			GBufferTexIdx_Count
		};
		static const std::array<char const*, GBufferTexIdx_Count> GBufferTexNames;


	public:
		GBufferGraphicsSystem(gr::GraphicsSystemManager*);

		~GBufferGraphicsSystem() override {}

		void InitPipeline(re::StagePipeline&);

		void PreRender();

		std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const override;


	private:
		void CreateBatches() override;

	private:
		std::shared_ptr<re::RenderStage> m_gBufferStage;
		re::StagePipeline* m_owningPipeline;
	};
}