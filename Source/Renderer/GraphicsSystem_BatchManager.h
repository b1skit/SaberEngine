// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "BufferView.h"
#include "Effect.h"
#include "RenderObjectIDs.h"
#include "GraphicsSystem.h"

#include "Core/Util/HashUtils.h"


struct RefCountedIndex;

namespace gr
{
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

		~BatchManagerGraphicsSystem() override = default;

		void InitPipeline(
			re::StagePipeline& pipeline, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);

		void PreRender();

		void EndOfFrame() override;


	private:
		void BuildViewBatches();


	private:
		// Instanced Transforms buffer name. Used to match Effects to Buffers
		static constexpr char const* k_instancedTransformBufferName = "InstancedTransforms";


	private:
		// We store our batches contiguously in a vector, and maintain a doubly-linked map to associate RenderDataIDs
		// with the associated cached batch indexes
		struct BatchMetadata
		{
			uint64_t m_batchHash;
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;
			EffectID m_matEffectID;
			size_t m_cacheIndex; // m_permanentCachedBatches
		};
		std::vector<re::Batch> m_permanentCachedBatches;
		std::unordered_map<gr::RenderDataID, BatchMetadata> m_renderDataIDToBatchMetadata;
		std::unordered_map<size_t, gr::RenderDataID> m_cacheIdxToRenderDataID;

		// Instanced Transforms: We maintain a single, monolithic Transform buffer, and index into it
		std::unordered_map<gr::TransformID, ::RefCountedIndex> m_instancedTransformIndexes;
		std::vector<uint32_t> m_freeTransformIndexes;
		re::BufferInput m_instancedTransforms;

		// Instanced Materials: We maintain a monolithic Material buffer per Material type, and index into it
		struct MaterialInstanceMetadata
		{
			std::unordered_map<gr::RenderDataID, ::RefCountedIndex> m_instancedMaterialIndexes;
			std::vector<uint32_t> m_freeInstancedMaterialIndexes;
			re::BufferInput m_instancedMaterials;
		};
		std::unordered_map<EffectID, MaterialInstanceMetadata> m_materialInstanceMetadata;

		ViewCullingResults const* m_viewCullingResults; // From the Culling GS
		AnimatedVertexStreams const* m_animatedVertexStreams; // From the vertex animation GS
		
		ViewBatches m_viewBatches; // Map of gr::Camera::View to vectors of Batches that passed culling
		std::vector<re::Batch> m_allBatches; // Per-frame copy of m_permanentCachedBatches, with Buffers set
		std::unordered_map<util::HashKey, re::BufferInput> m_instanceIndiciesBuffers;
	};
}