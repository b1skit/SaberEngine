// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"

#include "Core/InvPtr.h"


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
		~ReferencePathTracerGraphicsSystem() = default;

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		std::shared_ptr<re::AccelerationStructure> const* m_sceneTLAS;

		gr::StagePipeline* m_stagePipeline;

		std::shared_ptr<gr::Stage> m_rtStage;
		core::InvPtr<re::Texture> m_rtTarget;

		EffectID m_refPathTracerEffectID;

		uint32_t m_rayGenIdx;
		uint32_t m_missShaderIdx;
		uint8_t m_geometryInstanceMask;
	};
}