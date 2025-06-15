// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "Effect.h"
#include "RenderObjectIDs.h"
#include "GraphicsSystem.h"


namespace gr
{
	class IndexedBufferManager;


	// Our goal is to minimize the number of draw calls by instancing as many batches together as possible.
	// Theoretically, a system can afford to submit N batches per frame, the total number of triangles (or triangles
	// per batch) is far less important
	// https://www.nvidia.de/docs/IO/8230/BatchBatchBatch.pdf

	class BatchManagerGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<BatchManagerGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "BatchManager"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(BatchManagerGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(BatchManagerGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_cullingDataInput = "ViewCullingResults";
		static constexpr util::CHashKey k_animatedVertexStreamsInput = "AnimatedVertexStreams";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_viewBatchesDataOutput = "ViewBatches";
		static constexpr util::CHashKey k_allBatchesDataOutput = "AllBatches";
		void RegisterOutputs() override;


	public:
		BatchManagerGraphicsSystem(gr::GraphicsSystemManager*);

		void InitPipeline(
			re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();

		void EndOfFrame() override;


	private:
		void BuildViewBatches(gr::IndexedBufferManager&);


	private:
		// We store our batches contiguously in a vector, and maintain a doubly-linked map to associate RenderDataIDs
		// with the associated cached batch indexes
		struct BatchMetadata
		{
			uint64_t m_batchHash;
			gr::RenderDataID m_renderDataID;
			EffectID m_matEffectID;
			size_t m_cacheIndex; // m_permanentCachedBatches
		};
		std::vector<re::BatchHandle> m_permanentCachedBatches;
		std::unordered_map<gr::RenderDataID, BatchMetadata> m_renderDataIDToBatchMetadata;
		std::unordered_map<size_t, gr::RenderDataID> m_cacheIdxToRenderDataID;

		ViewCullingResults const* m_viewCullingResults; // From the Culling GS
		AnimatedVertexStreams const* m_animatedVertexStreams; // From the vertex animation GS
		
		ViewBatches m_viewBatches; // Map of gr::Camera::View to vectors of Batches that passed culling
		std::vector<re::BatchHandle> m_allBatches; // Per-frame copy of m_permanentCachedBatches, with Buffers set
	};
}