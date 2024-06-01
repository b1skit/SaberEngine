// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


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

		static constexpr char const* k_cullingDataInput = "ViewCullingResults";
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
		static constexpr std::array<char const*, GBufferTexIdx_Count> GBufferTexNames =
		{
			ENUM_TO_STR(GBufferAlbedo),		// 0
			ENUM_TO_STR(GBufferWNormal),	// 1
			ENUM_TO_STR(GBufferRMAO),		// 2
			ENUM_TO_STR(GBufferEmissive),	// 3
			ENUM_TO_STR(GBufferMatProp0),	// 4
			ENUM_TO_STR(GBufferDepth),		// 5
		};
		static_assert(GBufferGraphicsSystem::GBufferTexNames.size() ==
			GBufferGraphicsSystem::GBufferTexIdx::GBufferTexIdx_Count);
		// TODO: Split this into 2 lists: color target names, and depth names
		// -> Often need to loop over color, and treat depth differently


	public:
		GBufferGraphicsSystem(gr::GraphicsSystemManager*);

		~GBufferGraphicsSystem() override = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&);

		void PreRender(DataDependencies const&);


	private:
		void CreateBatches(DataDependencies const&);

	private:
		std::shared_ptr<re::RenderStage> m_gBufferStage;
		std::shared_ptr<re::TextureTargetSet> m_gBufferTargets;
		re::StagePipeline* m_owningPipeline;
	};
}