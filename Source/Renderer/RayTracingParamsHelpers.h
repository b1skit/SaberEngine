// © 2025 Adam Badke. All rights reserved.
#pragma once


enum RayFlag : uint32_t;
struct DescriptorIndexData;

namespace gr
{
	class IndexedBufferManager;
}
namespace re
{
	class AccelerationStructure;
	class Buffer;
	class BufferInput;
}

namespace grutil
{
	std::shared_ptr<re::Buffer> CreateTraceRayParams(
		uint8_t instanceInclusionMask,
		RayFlag rayFlags,
		uint32_t missShaderIdx,
		re::Buffer::StagingPool = re::Buffer::StagingPool::Temporary,
		re::Buffer::MemoryPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap);

	std::shared_ptr<re::Buffer> CreateTraceRayInlineParams(
		uint8_t instanceInclusionMask,
		RayFlag rayFlags,
		float tMin,
		float rayLengthOffset,
		re::Buffer::StagingPool = re::Buffer::StagingPool::Temporary,
		re::Buffer::MemoryPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap);

	std::shared_ptr<re::Buffer> CreateDescriptorIndexesBuffer(
		ResourceHandle vertexStreamLUTsDescriptorIdx,
		ResourceHandle instancedBufferLUTsDescriptorIdx,
		ResourceHandle cameraParamsDescriptorIdx,
		ResourceHandle targetUAVDescriptorIdx);

	re::BufferInput GetInstancedBufferLUTBufferInput(re::AccelerationStructure* tlas, gr::IndexedBufferManager&);
}