// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "RenderPipeline.h"

#include "Core/InvPtr.h"

#include "Core/Host/PerformanceTimer.h"


namespace re
{
	class AccelerationStructure;
	class ShaderBindingTable;
	class Texture;
}

namespace gr
{
	class GraphicsSystemManager;
	class StagePipeline;


	class ReferencePathTracerGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<ReferencePathTracerGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "ReferencePathTracer"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(ReferencePathTracerGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(ReferencePathTracerGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_sceneTLASInput = "SceneTLAS";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_rtTargetOutput = "RayTracingTarget";
		void RegisterOutputs() override;


	public:
		ReferencePathTracerGraphicsSystem(gr::GraphicsSystemManager*);
		~ReferencePathTracerGraphicsSystem();

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		void HandleEvents() override;


	private:
		std::shared_ptr<re::AccelerationStructure> const* m_sceneTLAS;

		gr::StagePipeline* m_stagePipeline;
		gr::StagePipeline::StagePipelineItr m_stagePipelineParentItr;

		std::shared_ptr<gr::Stage> m_rtStage;
		core::InvPtr<re::Texture> m_workingAccumulation;
		core::InvPtr<re::Texture> m_outputAccumulation;

		EffectID m_refPathTracerEffectID;

		uint32_t m_rayGenIdx;
		uint32_t m_missShaderIdx;
		uint8_t m_geometryInstanceMask;

		std::shared_ptr<re::Buffer> m_pathTracerParams;
		uint64_t m_accumulationStartFrame;
		uint32_t m_numAccumulatedFrames;
		uint32_t m_maxPathRays;
		bool m_mustResetTemporalAccumulation;

		core::InvPtr<re::Texture> m_environmentMap; // Spherical (latitude-longitude) environment map

		host::PerformanceTimer m_accumulationTimer;
	};
}