// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"


namespace gr
{
	class VertexAnimationGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<VertexAnimationGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "VertexAnimation"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(VertexAnimationGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(VertexAnimationGraphicsSystem, PreRender))
			);
		}

		static constexpr util::HashKey k_cullingDataInput = "ViewCullingResults";
		void RegisterInputs() override;

		static constexpr util::HashKey k_animatedVertexStreamsOutput = "AnimatedVertexStreams";
		void RegisterOutputs() override;


	public:
		VertexAnimationGraphicsSystem(gr::GraphicsSystemManager*);
		~VertexAnimationGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	private:
		std::shared_ptr<re::RenderStage> m_vertexAnimationStage;


	private:
		// Map from MeshConcept RenderDataID to animation data Buffers. Set on each Batch generated from MeshPrimitives
		// attached to the MeshConcept
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshIDToMeshRenderParams;
		

	public:
		// We maintain the shared_ptr<re::Buffer> lifetime, and also prepare VertexBufferInputs that can be used for
		// Batch construction by other GS's
		std::unordered_map<gr::RenderDataID, 
			std::array<std::shared_ptr<re::Buffer>, gr::VertexStream::k_maxVertexStreams>> m_meshPrimIDToDestBuffers;

		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshPrimIDToStreamMetadataBuffer;
		
		// Maintain this seperately: We share it with other GS's as a dependency output
		// Note: The contents here are always valid, even if the associated Batch did not pass culling. However,
		// the buffers are only actually updated/animated if the batch passed culling
		AnimatedVertexStreams m_outputs;

		void AddDestVertexBuffers(gr::RenderDataID, gr::MeshPrimitive::RenderData const&);
		void RemoveDestVertexBuffers(gr::RenderDataID);


	private:
		ViewCullingResults const* m_viewCullingResults; // From the Culling GS: We only animate things that pass culling
	};
}