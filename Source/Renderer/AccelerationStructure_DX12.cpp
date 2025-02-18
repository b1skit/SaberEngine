// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure_DX12.h"
#include "Buffer_DX12.h"
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "EnumTypes_DX12.h"

#include "Core/Util/HashUtils.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlagsToD3DBuildFlags(
		re::AccelerationStructure::BuildFlags flags)
	{
		SEStaticAssert(
			re::AccelerationStructure::BuildFlags::Default == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE &&
			re::AccelerationStructure::BuildFlags::AllowUpdate == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
			re::AccelerationStructure::BuildFlags::AllowCompaction == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION &&
			re::AccelerationStructure::BuildFlags::PreferFastTrace == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE &&
			re::AccelerationStructure::BuildFlags::PreferFastBuild == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD &&
			re::AccelerationStructure::BuildFlags::MinimizeMemory == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY &&
			re::AccelerationStructure::BuildFlags::PerformUpdate == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE,
			"Build flags out of sync");

		return static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(flags);
	}


	constexpr D3D12_RAYTRACING_GEOMETRY_FLAGS GeometryFlagsToD3DGeometryFlags(
		re::AccelerationStructure::GeometryFlags flags)
	{
		SEStaticAssert(
			re::AccelerationStructure::GeometryFlags::None == D3D12_RAYTRACING_GEOMETRY_FLAG_NONE &&
			re::AccelerationStructure::GeometryFlags::Opaque == D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE &&
			re::AccelerationStructure::GeometryFlags::NoDuplicateAnyHitInvocation == D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION,
			"Geometry flags out of sync");

		return static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(flags);
	}


	void ComputeASBufferSizes(
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& blasDesc,
		ID3D12Device5* device,
		uint64_t& resultDataMaxByteSizeOut,
		uint64_t& scratchDataByteSizeOut,
		uint64_t& updateScratchDataByteSizeOut)
	{
		SEAssert(device, "Invalid parameters for computing BLAS size");

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};

		device->GetRaytracingAccelerationStructurePrebuildInfo(&blasDesc, &prebuildInfo);

		resultDataMaxByteSizeOut = prebuildInfo.ResultDataMaxSizeInBytes;
		scratchDataByteSizeOut = prebuildInfo.ScratchDataSizeInBytes;
		updateScratchDataByteSizeOut = prebuildInfo.UpdateScratchDataSizeInBytes;
	}


	void CreateBLAS(re::AccelerationStructure& blas)
	{
		SEAssert(blas.GetType() == re::AccelerationStructure::Type::BLAS, "Invalid type");

		dx12::AccelerationStructure::PlatformParams* platParams =
			blas.GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams*>();

		re::AccelerationStructure::BLASCreateParams const* createParams = 
			dynamic_cast<re::AccelerationStructure::BLASCreateParams const*>(blas.GetCreateParams());
		SEAssert(createParams, "Failed to get AS create params");

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(createParams->m_instances.size());

		for (auto const& instance : createParams->m_instances)
		{
			// Currently, our re::Buffers have not been created/allocated at this point (they're staged in CPU memory,
			// and will be committed to GPU resources *after* RenderManager::CreateAPIResources). The DX12 AS prebuild
			// info structure doesn't dereference GPU pointers, but it does check if they're null or not when
			// computing the required buffer sizes. So here, we set a non-null GPU VA to ensure our buffer sizes are
			// correct, and then use the correct GPU VA when we're actually building our BLAS
			const D3D12_GPU_VIRTUAL_ADDRESS transform3x4DummyAddr = createParams->m_transform ? 1 : 0;

			const DXGI_FORMAT indexFormat = instance.m_indices ? 
				dx12::DataTypeToDXGI_FORMAT(instance.m_indices->GetDataType(), false) : DXGI_FORMAT_UNKNOWN;
			SEAssert(indexFormat == DXGI_FORMAT_UNKNOWN || 
				indexFormat == DXGI_FORMAT_R32_UINT ||
				indexFormat == DXGI_FORMAT_R16_UINT,
				"Invalid index format");
			
			const uint32_t indexCount = instance.m_indices ? instance.m_indices->GetNumElements() : 0;

			// Dummy GPU VA here for same reason as above
			const D3D12_GPU_VIRTUAL_ADDRESS indexBufferDummyAddr = instance.m_indices ? 1 : 0;

			const DXGI_FORMAT vertexFormat = dx12::DataTypeToDXGI_FORMAT(instance.m_positions->GetDataType(), false);
			const uint32_t vertexCount = instance.m_positions->GetNumElements();
			const D3D12_GPU_VIRTUAL_ADDRESS positionBufferDummyAddr = 1 ; // Dummy GPU VA here for same reason as above

			geometryDescs.emplace_back(D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
				.Flags = GeometryFlagsToD3DGeometryFlags(instance.m_geometryFlags),
				.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC{
					.Transform3x4 = transform3x4DummyAddr,
					.IndexFormat = indexFormat,
					.VertexFormat = vertexFormat,
					.IndexCount = indexCount,
					.VertexCount = vertexCount,
					.IndexBuffer = indexBufferDummyAddr,
					.VertexBuffer = positionBufferDummyAddr, }
				});
		}
		
		// Compute the estimated buffer sizes:
		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{
			.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
			.Flags = BuildFlagsToD3DBuildFlags(createParams->m_buildFlags),
			.NumDescs = util::CheckedCast<uint32_t>(geometryDescs.size()),
			.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, // geometryDescs holds D3D12_RAYTRACING_GEOMETRY_DESC objects directly
			.pGeometryDescs = geometryDescs.data(),
		};
		uint64_t resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize;
		ComputeASBufferSizes(blasInputs, platParams->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);
		
		// Create a the BLAS buffer:
		platParams->m_ASBuffer = platParams->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					resultDataMaxByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, },
			util::ToWideString(blas.GetName()).c_str());
	}


	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildBLASDesc(re::AccelerationStructure& blas, bool doUpdate)
	{
		SEAssert(blas.GetType() == re::AccelerationStructure::Type::BLAS, "Invalid type");

		dx12::AccelerationStructure::PlatformParams* platParams =
			blas.GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams*>();

		SEAssert(platParams->m_ASBuffer, "BLAS buffer is null. This should not be possible");

		re::AccelerationStructure::BLASCreateParams const* createParams =
			dynamic_cast<re::AccelerationStructure::BLASCreateParams const*>(blas.GetCreateParams());
		SEAssert(createParams, "Failed to get AS create params");

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(createParams->m_instances.size());

		for (size_t instanceIdx = 0; instanceIdx < createParams->m_instances.size(); ++instanceIdx)
		{
			re::AccelerationStructure::BLASCreateParams::Instance const& instance = createParams->m_instances[instanceIdx];

			// Transform:
			D3D12_GPU_VIRTUAL_ADDRESS transform3x4Addr = NULL;
			if (createParams->m_transform)
			{
				transform3x4Addr = dx12::Buffer::GetGPUVirtualAddress(createParams->m_transform.get());

				constexpr uint32_t transformByteSize = 16 * 3 * 4;
				transform3x4Addr += static_cast<uint64_t>(instanceIdx) * transformByteSize;

				SEAssert(transform3x4Addr % D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT == 0,
					"Transform addresses must be aligned to 16 bytes");
			}

			// Indices:
			DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
			uint32_t indexCount = 0;
			D3D12_GPU_VIRTUAL_ADDRESS indexBufferAddr = NULL;
			if (instance.m_indices)
			{
				indexFormat = dx12::DataTypeToDXGI_FORMAT(instance.m_indices->GetDataType(), false);
				SEAssert(indexFormat == DXGI_FORMAT_UNKNOWN ||
					indexFormat == DXGI_FORMAT_R32_UINT ||
					indexFormat == DXGI_FORMAT_R16_UINT,
					"Invalid index format");

				indexCount = instance.m_indices->GetNumElements();
				indexBufferAddr = dx12::Buffer::GetGPUVirtualAddress(instance.m_indices->GetBuffer());
			}

			// Positions:
			const DXGI_FORMAT vertexFormat = dx12::DataTypeToDXGI_FORMAT(instance.m_positions->GetDataType(), false);
			const uint32_t vertexCount = instance.m_positions->GetNumElements();
			const D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE positionBufferAddr = D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE{
				.StartAddress = dx12::Buffer::GetGPUVirtualAddress(instance.m_positions->GetBuffer()),
				.StrideInBytes = re::DataTypeToByteStride(instance.m_positions->GetDataType()), };

			geometryDescs.emplace_back(D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
				.Flags = GeometryFlagsToD3DGeometryFlags(instance.m_geometryFlags),
				.Triangles = D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC{
					.Transform3x4 = transform3x4Addr,
					.IndexFormat = indexFormat,
					.VertexFormat = vertexFormat,
					.IndexCount = indexCount,
					.VertexCount = vertexCount,
					.IndexBuffer = indexBufferAddr,
					.VertexBuffer = positionBufferAddr, }
				});
		}

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = BuildFlagsToD3DBuildFlags(createParams->m_buildFlags);
		if (doUpdate)
		{
			SEAssert(createParams->m_buildFlags & re::AccelerationStructure::BuildFlags::AllowUpdate,
				"Trying to update a BLAS, but the build flags don't have the AllowUpdate bit set");

			flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}

		// Compute the estimated buffer sizes:
		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{
			.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
			.Flags = flags,
			.NumDescs = util::CheckedCast<uint32_t>(geometryDescs.size()),
			.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, // geometryDescs holds D3D12_RAYTRACING_GEOMETRY_DESC objects directly
			.pGeometryDescs = geometryDescs.data(),
		};
		uint64_t resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize;
		ComputeASBufferSizes(blasInputs, platParams->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);

		// Create a temporary scratch buffer:	
		// Note: We allow the scratch buffer to immediately go out of scope and rely on the the dx12::HeapManager 
		// deferred deletion to guarantee  lifetime
		std::unique_ptr<dx12::GPUResource> scratchBuffer = platParams->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					scratchDataByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_COMMON, },
			L"BuildBLAS temporary scratch buffer");

		// Generate/update the BLAS:
		return D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC{
			.DestAccelerationStructureData = platParams->m_ASBuffer->GetGPUVirtualAddress(),
			.Inputs = blasInputs,
			.SourceAccelerationStructureData = doUpdate ? platParams->m_ASBuffer->GetGPUVirtualAddress() : NULL,
			.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress(),
		};
	}
}

namespace dx12
{
	AccelerationStructure::PlatformParams::PlatformParams()
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		m_heapManager = &context->GetHeapManager();
		
		Microsoft::WRL::ComPtr<ID3D12Device5> device5;
		const HRESULT hr = context->GetDevice().GetD3DDevice().As(&device5);
		SEAssert(SUCCEEDED(hr), "Failed to get device5");

		m_device = device5.Get();
	}


	void AccelerationStructure::PlatformParams::Destroy()
	{
		m_heapManager = nullptr;
		m_device = nullptr;
		m_ASBuffer = nullptr;
	}


	void AccelerationStructure::Create(re::AccelerationStructure& as)
	{
		// We create our acceleration structure buffers in advance to ensure they're valid (albeit uninitialized) during
		// asyncronous command list recording. This prevents a potential race condition where a thread recording a
		// command list tries to set an acceleration structure before another thread creates it.
		switch (as.GetType())
		{
		case re::AccelerationStructure::Type::TLAS:
		{

		}
		break;
		case re::AccelerationStructure::Type::BLAS:
		{
			CreateBLAS(as);
		}
		break;
		default: SEAssertF("Invalid AccelerationStructure Type");
		}
	}


	void AccelerationStructure::Destroy(re::AccelerationStructure& as)
	{
		dx12::AccelerationStructure::PlatformParams* platParams =
			as.GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams*>();
	}


	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC AccelerationStructure::BuildAccelerationStructureDesc(
		re::AccelerationStructure& as)
	{
		switch (as.GetType())
		{
		case re::AccelerationStructure::Type::TLAS:
		{

		}
		break;
		case re::AccelerationStructure::Type::BLAS:
		{
			return BuildBLASDesc(as, false);
		}
		break;
		default: SEAssertF("Invalid AccelerationStructure Type");
		}
		return {}; // This should never happen
	}
}