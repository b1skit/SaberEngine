// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"

#include "Core/Util/CHashKey.h"


namespace re
{
	class MeshPrimitive;
	class Texture;
}

namespace gr
{
	class StagePipeline;


	class DeferredUnlitGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<DeferredUnlitGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "DeferredUnlit"; }


		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE
				(
					INIT_PIPELINE_FN(DeferredUnlitGraphicsSystem, InitPipeline)
				)
				PRE_RENDER(PRE_RENDER_FN(DeferredUnlitGraphicsSystem, PreRender))
			);
		}


	public:
		// Note: The DeferredUnlitGraphicsSystem uses GBufferGraphicsSystem::GBufferTexNames for its inputs
		void RegisterInputs() override;

		static constexpr util::CHashKey k_lightingTargetTexOutput = "LightTargetTex";
		void RegisterOutputs() override;


	public:
		DeferredUnlitGraphicsSystem(gr::GraphicsSystemManager*);

		~DeferredUnlitGraphicsSystem() override = default;

		void InitPipeline(
			gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();


	private:
		std::shared_ptr<gr::Stage> m_fullscreenStage;
		gr::BatchHandle m_fullscreenComputeBatch;
		static constexpr uint32_t k_dispatchXYThreadDims = 8;


	private: // Common:
		std::shared_ptr<re::TextureTargetSet> m_primaryLightingTargetSet;
	};
}