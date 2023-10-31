// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "Mesh.h"
#include "MeshPrimitive.h"


namespace re
{
	// TODO: For now, this is just stubbed in. Eventually, this should handle Batch allocations, single-frame lifetimes,
	// etc
	class BatchManager
	{
	public:
		// Our goal is to minimize the number of draw calls by instancing as many batches together as possible.
		// Theoretically, a system can afford to submit N batches per frame, the total number of triangles (or triangles
		// per batch) is far less important
		// https://www.nvidia.de/docs/IO/8230/BatchBatchBatch.pdf

		static std::vector<re::Batch> BuildBatches(std::vector<std::shared_ptr<gr::Mesh>> const&);
	};
}


