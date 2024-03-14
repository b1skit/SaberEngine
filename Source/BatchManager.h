// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class RenderDataManager;
}

namespace gr
{
	class BatchManager
	{
		// Our goal is to minimize the number of draw calls by instancing as many batches together as possible.
		// Theoretically, a system can afford to submit N batches per frame, the total number of triangles (or triangles
		// per batch) is far less important
		// https://www.nvidia.de/docs/IO/8230/BatchBatchBatch.pdf
	
	public:
		enum InstanceType : uint8_t // Bitmask helper: Which buffers to attach to batches?
		{
			Transform	= 0x1,
			Material	= 0x2
		};

		
	public:
		BatchManager();
		~BatchManager() = default;


		void UpdateBatchCache(gr::RenderDataManager const&);

	public:
		// Build a vector of single frame scene batches from the vector of RenderDataIDs, from the interal batch cache
		std::vector<re::Batch> BuildSceneBatches(
			gr::RenderDataManager const&,
			std::vector<gr::RenderDataID> const&,
			uint8_t bufferTypeMask = (InstanceType::Transform | InstanceType::Material)) const;


	public:
		struct RefCountedIndex
		{
			uint32_t m_index;
			uint32_t m_refCount;
		};


	private:
		// We store our batches contiguously in a vector, and maintain a doubly-linked map to associate RenderDataIDs
		// with the associated cached batch indexes
		struct BatchMetadata
		{
			uint64_t m_batchHash;
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;
			size_t m_cacheIndex; // m_permanentCachedBatches
		};
		std::vector<re::Batch> m_permanentCachedBatches;
		std::unordered_map<gr::RenderDataID, BatchMetadata> m_renderDataIDToBatchMetadata;
		std::unordered_map<size_t, gr::RenderDataID> m_cacheIdxToRenderDataID;

		// Instancing:
		std::unordered_map<gr::TransformID, RefCountedIndex> m_instancedTransformIndexes;
		std::vector<uint32_t> m_freeTransformIndexes;
		std::shared_ptr<re::Buffer> m_instancedTransforms;

		std::unordered_map<gr::RenderDataID, RefCountedIndex> m_instancedMaterialIndexes;
		std::vector<uint32_t> m_freeInstancedMaterialIndexes;
		std::shared_ptr<re::Buffer> m_instancedMaterials;
	};
}


