// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"

#include "Core/Assert.h"


namespace re
{
	class RenderStage;
}

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

		static constexpr util::HashKey k_viewBatchesDataInput = "ViewBatches";
		static constexpr util::HashKey k_allBatchesDataInput = "AllBatches";
		void RegisterInputs() override;
		void RegisterOutputs() override;


	public:
		// These enums must align with the layout binding indexes defined in SaberCommon.glsl
		enum GBufferTexIdx : uint8_t
		{
			GBufferAlbedo	= 0,
			GBufferWNormal	= 1,
			GBufferRMAO		= 2,
			GBufferEmissive = 3,
			GBufferMatProp0 = 4,

			GBufferDepth	= 5,

			GBufferTexIdx_Count,
			GBufferColorTex_Count = 5 // Helper for iterating over color indexes only
		};
		static constexpr std::array<util::HashKey, GBufferTexIdx_Count> GBufferTexNameHashKeys =
		{
			util::HashKey(ENUM_TO_STR(GBufferAlbedo)),		// 0
			util::HashKey(ENUM_TO_STR(GBufferWNormal)),		// 1
			util::HashKey(ENUM_TO_STR(GBufferRMAO)),		// 2
			util::HashKey(ENUM_TO_STR(GBufferEmissive)),	// 3
			util::HashKey(ENUM_TO_STR(GBufferMatProp0)),	// 4
			util::HashKey(ENUM_TO_STR(GBufferDepth)),		// 5
		};
		SEStaticAssert(GBufferGraphicsSystem::GBufferTexNameHashKeys.size() ==
			GBufferGraphicsSystem::GBufferTexIdx::GBufferTexIdx_Count,
			"GBuffer hash keys are out of sync with enum");


	public:
		GBufferGraphicsSystem(gr::GraphicsSystemManager*);

		~GBufferGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		void CreateBatches();

	private:
		std::shared_ptr<re::RenderStage> m_gBufferStage;
		std::shared_ptr<re::TextureTargetSet> m_gBufferTargets;
		re::StagePipeline* m_owningPipeline;

	private: // Cached dependencies:
		ViewBatches const* m_viewBatches;
		AllBatches const* m_allBatches;
	};
}