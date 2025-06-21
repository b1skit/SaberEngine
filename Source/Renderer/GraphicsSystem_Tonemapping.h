// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace gr
{
	class TonemappingGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<TonemappingGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "Tonemapping"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(TonemappingGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(TonemappingGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_tonemappingTargetInput = "TonemappingTarget";
		static constexpr util::CHashKey k_bloomResultInput = "BloomResult";
		void RegisterInputs() override;

		void RegisterOutputs() override;


	public:
		TonemappingGraphicsSystem(gr::GraphicsSystemManager*);

		~TonemappingGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		std::shared_ptr<re::Stage> m_tonemappingStage;
		gr::BatchHandle m_tonemappingComputeBatch;
		static constexpr uint32_t k_dispatchXYThreadDims = 8;

		std::shared_ptr<re::Stage> m_swapchainBlitStage; // Fullscreen quad stage


	private:
		enum TonemappingMode : uint8_t
		{
			ACES,
			ACES_FAST,
			Reinhard,
			PassThrough,

			Count
		};
		TonemappingMode m_currentMode;
	};
}