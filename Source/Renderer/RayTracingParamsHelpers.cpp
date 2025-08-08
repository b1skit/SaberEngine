// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "Buffer.h"
#include "BufferView.h"
#include "Effect.h"
#include "IndexedBuffer.h"
#include "RayTracingParamsHelpers.h"

#include "Shaders/Common/MaterialParams.h"
#include "Shaders/Common/RayTracingParams.h"
#include "Shaders/Common/TransformParams.h"


namespace grutil
{
	std::shared_ptr<re::Buffer> CreateTraceRayParams(
		uint8_t instanceInclusionMask,
		RayFlag rayFlags,
		uint32_t missShaderIdx,
		re::Buffer::StagingPool stagingPool /*= re::Buffer::StagingPool::Temporary*/,
		re::Buffer::MemoryPoolPreference memPoolPref /*= re::Buffer::MemoryPoolPreference::UploadHeap*/)
	{
		SEAssert(instanceInclusionMask <= 0xFF, "Instance inclusion mask has maximum 8 bits");

		const TraceRayData traceRayData{
			.g_traceRayParams = glm::uvec4(
				static_cast<uint32_t>(instanceInclusionMask),	// InstanceInclusionMask
				0,												// RayContributionToHitGroupIndex
				0,												// MultiplierForGeometryContributionToHitGroupIndex
				missShaderIdx),									// MissShaderIndex
			.g_rayFlags = glm::uvec4(
				rayFlags,
				0,
				0,
				0),
		};

		re::Buffer::Access accessMask = re::Buffer::Access::GPURead;
		if (memPoolPref == re::Buffer::MemoryPoolPreference::UploadHeap)
		{
			accessMask |= re::Buffer::Access::CPUWrite;			
		}


		const re::Buffer::BufferParams traceRayBufferParams{
			.m_lifetime = re::Lifetime::SingleFrame,
			.m_stagingPool = stagingPool,
			.m_memPoolPreference = memPoolPref,
			.m_accessMask = accessMask,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Trace Ray Params", traceRayData, traceRayBufferParams);
	}


	std::shared_ptr<re::Buffer> CreateTraceRayInlineParams(
		uint8_t instanceInclusionMask,
		RayFlag rayFlags,
		float tMin,
		float rayLengthOffset,
		re::Buffer::StagingPool stagingPool /*= re::Buffer::StagingPool::Temporary*/,
		re::Buffer::MemoryPoolPreference memPoolPref /*= re::Buffer::MemoryPoolPreference::UploadHeap*/)
	{
		SEAssert(instanceInclusionMask <= 0xFF, "Instance inclusion mask has maximum 8 bits");

		const TraceRayInlineData traceRayInlineData{
			.g_traceRayInlineParams = glm::uvec4(
				static_cast<uint32_t>(instanceInclusionMask),	// InstanceInclusionMask
				rayFlags,										
				0,
				0),
			.g_rayParams = glm::vec4(
				tMin,
				rayLengthOffset,
				0.f,
				0.f),
		};

		re::Buffer::Access accessMask = re::Buffer::Access::GPURead;
		if (memPoolPref == re::Buffer::MemoryPoolPreference::UploadHeap)
		{
			accessMask |= re::Buffer::Access::CPUWrite;
		}

		const re::Buffer::BufferParams traceRayInlineBufferParams{
			.m_lifetime = re::Lifetime::SingleFrame,
			.m_stagingPool = stagingPool,
			.m_memPoolPreference = memPoolPref,
			.m_accessMask = accessMask,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Trace Ray Inline Params", traceRayInlineData, traceRayInlineBufferParams);
	}


	std::shared_ptr<re::Buffer> CreateDescriptorIndexesBuffer(
		ResourceHandle vertexStreamLUTsDescriptorIdx,
		ResourceHandle instancedBufferLUTsDescriptorIdx,
		ResourceHandle cameraParamsDescriptorIdx,
		ResourceHandle targetUAVDescriptorIdx)
	{
		SEAssert(vertexStreamLUTsDescriptorIdx != INVALID_RESOURCE_IDX &&
			instancedBufferLUTsDescriptorIdx != INVALID_RESOURCE_IDX &&
			cameraParamsDescriptorIdx != INVALID_RESOURCE_IDX &&
			targetUAVDescriptorIdx != INVALID_RESOURCE_IDX,
			"Descriptor index is invalid. This is unexpected");

		// .x = VertexStreamLUTs, .y = InstancedBufferLUTs, .z = CameraParams, .w = output Texture2DRWFloat4 idx
		const DescriptorIndexData descriptorIndexData{
			.g_descriptorIndexes = glm::uvec4(
				vertexStreamLUTsDescriptorIdx,		// VertexStreamLUTs[]
				instancedBufferLUTsDescriptorIdx,	// InstancedBufferLUTs[]
				cameraParamsDescriptorIdx,			// CameraParams[]
				targetUAVDescriptorIdx),			// Texture2DRWFloat4[]
		};

		const re::Buffer::BufferParams descriptorIndexParams{
			.m_lifetime = re::Lifetime::SingleFrame,
			.m_stagingPool = re::Buffer::StagingPool::Temporary,
			.m_memPoolPreference = re::Buffer::MemoryPoolPreference::UploadHeap,
			.m_accessMask = re::Buffer::Access::GPURead | re::Buffer::Access::CPUWrite,
			.m_usageMask = re::Buffer::Usage::Constant,
		};

		return re::Buffer::Create("Descriptor Indexes", descriptorIndexData, descriptorIndexParams);
	}


	re::BufferInput GetInstancedBufferLUTBufferInput(re::AccelerationStructure* tlas, gr::IndexedBufferManager& ibm)
	{
		SEAssert(tlas, "Pointer is null");

		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas->GetASParams());

		const ResourceHandle transformBufferHandle =
			ibm.GetIndexedBuffer(TransformData::s_shaderName)->GetResourceHandle(re::ViewType::SRV);
		const ResourceHandle unlitMaterialBufferHandle =
			ibm.GetIndexedBuffer(UnlitData::s_shaderName)->GetResourceHandle(re::ViewType::SRV);
		const ResourceHandle pbrMetRoughMaterialBufferHandle =
			ibm.GetIndexedBuffer(PBRMetallicRoughnessData::s_shaderName)->GetResourceHandle(re::ViewType::SRV);

		std::vector<uint32_t> const& blasGeoIDs = tlasParams->GetBLASGeometryOwnerIDs();

		size_t geoIdx = 0;
		std::vector<InstancedBufferLUTData> initialLUTData;
		for (auto const& blas : tlasParams->GetBLASInstances())
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());

			for (auto const& geometry : blasParams->m_geometry)
			{
				SEAssert(blasGeoIDs[geoIdx++] == geometry.GetOwnerID(), "Geometry and IDs are out of sync");

				effect::Effect const* geoEffect = geometry.GetEffectID().GetEffect();
				ResourceHandle materialResourceHandle = INVALID_RESOURCE_IDX;
				if (geoEffect->UsesBuffer(PBRMetallicRoughnessData::s_shaderName))
				{
					materialResourceHandle = pbrMetRoughMaterialBufferHandle;
				}
				else if (geoEffect->UsesBuffer(UnlitData::s_shaderName))
				{
					materialResourceHandle = unlitMaterialBufferHandle;
				}
				SEAssert(materialResourceHandle != INVALID_RESOURCE_IDX, "Failed to find a material resource handle");

				SEAssert(geometry.GetEffectID().GetEffect()->UsesBuffer(TransformData::s_shaderName),
					"Effect does not use TransformData. This is unexpected");

				initialLUTData.emplace_back(InstancedBufferLUTData{
					.g_materialIndexes = glm::uvec4(materialResourceHandle, 0, 0, 0),
					.g_transformIndexes = glm::uvec4(transformBufferHandle, 0, 0, 0),
					});
			}
		}
		return ibm.GetLUTBufferInput<InstancedBufferLUTData>(
			InstancedBufferLUTData::s_shaderName, std::move(initialLUTData), blasGeoIDs);
	}
}