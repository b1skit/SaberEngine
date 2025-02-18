// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"


namespace re
{
	class AccelerationStructure;
}

namespace gr
{
	class AccelerationStructuresGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<AccelerationStructuresGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "AccelerationStructures"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(AccelerationStructuresGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(AccelerationStructuresGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_animatedVertexStreamsInput = "AnimatedVertexStreams";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_sceneTLASOutput = "SceneTLAS";
		void RegisterOutputs() override;


	public:
		AccelerationStructuresGraphicsSystem(gr::GraphicsSystemManager*);
		~AccelerationStructuresGraphicsSystem();

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();

		void ShowImGuiWindow() override;


	private:
		std::shared_ptr<re::AccelerationStructure> m_sceneTLAS;

		// BLAS tracking:
		std::unordered_map<gr::RenderDataID, std::unordered_set<gr::RenderDataID>> m_meshConceptToPrimitiveIDs;
		std::unordered_map<gr::RenderDataID, gr::RenderDataID> m_meshPrimToMeshConceptID;
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::AccelerationStructure>> m_meshConceptToBLAS;

		re::StagePipeline* m_stagePipeline;
		re::StagePipeline::StagePipelineItr m_rtParentStageItr;

		AnimatedVertexStreams const* m_animatedVertexStreams; // From the vertex animation GS
	};
}