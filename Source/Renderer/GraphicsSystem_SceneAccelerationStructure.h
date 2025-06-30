// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "RenderObjectIDs.h"
#include "RenderPipeline.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/HashKey.h"


namespace re
{
	class AccelerationStructure;
}

namespace gr
{
	class SceneAccelerationStructureGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<SceneAccelerationStructureGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "SceneAccelerationStructure"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(SceneAccelerationStructureGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(SceneAccelerationStructureGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_animatedVertexStreamsInput = "AnimatedVertexStreams";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_sceneTLASOutput = "SceneTLAS";
		void RegisterOutputs() override;


	public:
		SceneAccelerationStructureGraphicsSystem(gr::GraphicsSystemManager*);
		~SceneAccelerationStructureGraphicsSystem();

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();

		void ShowImGuiWindow() override;


	private:
		std::shared_ptr<re::AccelerationStructure> m_sceneTLAS;

		// BLAS tracking:
		std::unordered_map<gr::RenderDataID, std::unordered_set<gr::RenderDataID>> m_meshConceptToPrimitiveIDs;
		std::unordered_map<gr::RenderDataID, gr::RenderDataID> m_meshPrimToMeshConceptID;
		
		std::unordered_map<gr::RenderDataID, // MeshConcept RenderDataID
			std::map<util::HashKey, // BLAS key
				std::pair<std::shared_ptr<re::AccelerationStructure>, uint32_t>>> m_meshConceptToBLASAndCount;

		std::unordered_map<gr::RenderDataID, util::HashKey> m_meshPrimToBLASKey;

		gr::StagePipeline* m_stagePipeline;
		gr::StagePipeline::StagePipelineItr m_rtParentStageItr;

		AnimatedVertexStreams const* m_animatedVertexStreams; // From the vertex animation GS
	};
}