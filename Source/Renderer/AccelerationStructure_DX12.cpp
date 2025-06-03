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
#if 0
	void DebugPrintASDesc(char const* asName, D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc, bool isUpdate)
	{
		LOG_ERROR("----------\n");
		LOG_ERROR(asName);

		LOG_ERROR(std::format("Is update? {}", isUpdate).c_str());

		LOG_ERROR(std::format("DestAccelerationStructureData = {}", desc.DestAccelerationStructureData).c_str());

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& inputs = desc.Inputs;
		LOG_ERROR(std::format("inputs.Type = {}", (uint64_t)inputs.Type).c_str());
		LOG_ERROR(std::format("inputs.Flags = {}", (uint64_t)inputs.Flags).c_str());
		LOG_ERROR(std::format("inputs.NumDescs = {}", inputs.NumDescs).c_str());
		LOG_ERROR(std::format("inputs.DescsLayout = {}", (uint64_t)inputs.DescsLayout).c_str());

		if (inputs.Type == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL)
		{
			for (size_t i = 0; i < inputs.NumDescs; ++i)
			{
				D3D12_RAYTRACING_GEOMETRY_DESC const& geoDesc = inputs.pGeometryDescs[i];

				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Type = {}", i, (uint64_t)inputs.pGeometryDescs[i].Type).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Flags = {}", i, (uint64_t)inputs.pGeometryDescs[i].Flags).c_str());

				D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC const& triDesc = geoDesc.Triangles;
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.Transform3x4 = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.Transform3x4).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.IndexFormat = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.IndexFormat).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.VertexFormat = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.VertexFormat).c_str());

				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.IndexCount = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.IndexCount).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.VertexCount = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.VertexCount).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.IndexBuffer = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.IndexBuffer).c_str());

				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.VertexBuffer.StartAddress = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.VertexBuffer.StartAddress).c_str());
				LOG_ERROR(std::format("inputs.pGeometryDescs[{}].Triangles.VertexBuffer.StrideInBytes = {}", i, (uint64_t)inputs.pGeometryDescs[i].Triangles.VertexBuffer.StrideInBytes).c_str());
			}
		}
		else // D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL
		{
			LOG_ERROR(std::format("inputs.InstanceDescs = {}", (uint64_t)inputs.InstanceDescs).c_str());
		}

		LOG_ERROR(std::format("SourceAccelerationStructureData = {}", (uint64_t)desc.SourceAccelerationStructureData).c_str());
		LOG_ERROR(std::format("ScratchAccelerationStructureData = {}", (uint64_t)desc.ScratchAccelerationStructureData).c_str());

		LOG_ERROR("----------\n");
	}
#endif

	constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlagsToD3DBuildFlags(
		re::AccelerationStructure::BuildFlags flags)
	{
		SEStaticAssert(
			re::AccelerationStructure::BuildFlags::BuildFlags_None == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE &&
			re::AccelerationStructure::BuildFlags::AllowUpdate == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE &&
			re::AccelerationStructure::BuildFlags::AllowCompaction == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION &&
			re::AccelerationStructure::BuildFlags::PreferFastTrace == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE &&
			re::AccelerationStructure::BuildFlags::PreferFastBuild == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD &&
			re::AccelerationStructure::BuildFlags::MinimizeMemory == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY/* &&
			re::AccelerationStructure::BuildFlags::PerformUpdate == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE*/,
			"Build flags out of sync");

		return static_cast<D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS>(flags);
	}


	constexpr D3D12_RAYTRACING_GEOMETRY_FLAGS GeometryFlagsToD3DGeometryFlags(
		re::AccelerationStructure::GeometryFlags flags)
	{
		SEStaticAssert(
			re::AccelerationStructure::GeometryFlags::GeometryFlags_None == D3D12_RAYTRACING_GEOMETRY_FLAG_NONE &&
			re::AccelerationStructure::GeometryFlags::Opaque == D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE &&
			re::AccelerationStructure::GeometryFlags::NoDuplicateAnyHitInvocation == D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION,
			"Geometry flags out of sync");

		return static_cast<D3D12_RAYTRACING_GEOMETRY_FLAGS>(flags);
	}


	constexpr D3D12_RAYTRACING_INSTANCE_FLAGS InstanceFlagsToD3DInstanceFlags(
		re::AccelerationStructure::InstanceFlags flags)
	{
		SEStaticAssert(
			re::AccelerationStructure::InstanceFlags::InstanceFlags_None == D3D12_RAYTRACING_INSTANCE_FLAG_NONE &&
			re::AccelerationStructure::InstanceFlags::TriangleCullDisable == D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE &&
			re::AccelerationStructure::InstanceFlags::TriangleFrontCounterClockwise == D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE &&
			re::AccelerationStructure::InstanceFlags::ForceOpaque == D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE &&
			re::AccelerationStructure::InstanceFlags::ForceNonOpaque == D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE,
			"Instance flags out of sync");

		return static_cast<D3D12_RAYTRACING_INSTANCE_FLAGS>(flags);
	}


	// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_raytracing_acceleration_structure_prebuild_info
	void ComputeASBufferSizes(
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& asDesc,
		ID3D12Device5* device,
		uint64_t& resultDataMaxByteSizeOut,
		uint64_t& scratchDataByteSizeOut,
		uint64_t& updateScratchDataByteSizeOut)
	{
		SEAssert(device, "Invalid parameters for computing BLAS size");

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};

		device->GetRaytracingAccelerationStructurePrebuildInfo(&asDesc, &prebuildInfo);

		// Align buffers to 256B / D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT:
		resultDataMaxByteSizeOut = util::RoundUpToNearestMultiple<uint64_t>(
			prebuildInfo.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		scratchDataByteSizeOut = util::RoundUpToNearestMultiple<uint64_t>(
			prebuildInfo.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		updateScratchDataByteSizeOut = util::RoundUpToNearestMultiple<uint64_t>(
			prebuildInfo.UpdateScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}


	uint64_t ComputeTLASInstancesBufferSize(re::AccelerationStructure::TLASParams const* tlasParams)
	{
		// Compute the size of the BLAS instance descriptors that will be stored in GPU memory:
		return util::RoundUpToNearestMultiple<uint64_t>(
			sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * tlasParams->GetBLASInstances().size(),
			D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}


	void CreateBLASResources(re::AccelerationStructure& blas)
	{
		SEAssert(blas.GetType() == re::AccelerationStructure::Type::BLAS, "Invalid type");

		dx12::AccelerationStructure::PlatObj* blasPlatObj =
			blas.GetPlatformObject()->As<dx12::AccelerationStructure::PlatObj*>();

		re::AccelerationStructure::BLASParams const* blasParams = 
			dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas.GetASParams());
		SEAssert(blasParams, "Failed to get BLAS params");

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(blasParams->m_geometry.size());

		for (auto const& instance : blasParams->m_geometry)
		{
			SEAssert(instance.GetVertexIndices()->GetType() == gr::VertexStream::Type::Index &&
				instance.GetVertexPositions().GetStream()->GetType() == gr::VertexStream::Type::Position,
				"Invalid vertex stream geometry inputs");

			// Currently, our re::Buffers have not been created/allocated at this point (they're staged in CPU memory,
			// and will be committed to GPU resources *after* RenderManager::CreateAPIResources). The DX12 AS prebuild
			// info structure doesn't dereference GPU pointers, but it does check if they're null or not when
			// computing the required buffer sizes. So here, we set a non-null GPU VA to ensure our buffer sizes are
			// correct, and then use the correct GPU VA when we're actually building our BLAS
			const D3D12_GPU_VIRTUAL_ADDRESS transform3x4DummyAddr = blasParams->m_transform ? 1 : 0;

			const DXGI_FORMAT indexFormat = instance.GetVertexIndices() ? 
				dx12::DataTypeToDXGI_FORMAT(instance.GetVertexIndices()->GetDataType(), false) : DXGI_FORMAT_UNKNOWN;
			SEAssert(indexFormat == DXGI_FORMAT_UNKNOWN || 
				indexFormat == DXGI_FORMAT_R32_UINT ||
				indexFormat == DXGI_FORMAT_R16_UINT,
				"Invalid index format");
			
			const uint32_t indexCount = instance.GetVertexIndices() ? instance.GetVertexIndices()->GetNumElements() : 0;

			// Dummy GPU VA here for same reason as above
			const D3D12_GPU_VIRTUAL_ADDRESS indexBufferDummyAddr = instance.GetVertexIndices() ? 1 : 0;

			const DXGI_FORMAT vertexFormat =
				dx12::DataTypeToDXGI_FORMAT(instance.GetVertexPositions().GetStream()->GetDataType(), false);
			const uint32_t vertexCount = instance.GetVertexPositions().GetStream()->GetNumElements();
			const D3D12_GPU_VIRTUAL_ADDRESS positionBufferDummyAddr = 1 ; // Dummy GPU VA here for same reason as above

			geometryDescs.emplace_back(D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
				.Flags = GeometryFlagsToD3DGeometryFlags(instance.GetGeometryFlags()),
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
			.Flags = BuildFlagsToD3DBuildFlags(blasParams->m_buildFlags),
			.NumDescs = util::CheckedCast<uint32_t>(geometryDescs.size()),
			.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, // geometryDescs holds D3D12_RAYTRACING_GEOMETRY_DESC objects directly
			.pGeometryDescs = geometryDescs.data(),
		};
		uint64_t resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize;
		ComputeASBufferSizes(blasInputs, blasPlatObj->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);

		// Create a the BLAS buffer:
		blasPlatObj->m_ASBuffer = blasPlatObj->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					resultDataMaxByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, },
			util::ToWideString(blas.GetName()).c_str());
	}


	void BuildBLAS(re::AccelerationStructure& blas, bool doUpdate, ID3D12GraphicsCommandList4* cmdList)
	{
		SEAssert(blas.GetType() == re::AccelerationStructure::Type::BLAS, "Invalid type");

		dx12::AccelerationStructure::PlatObj* platObj =
			blas.GetPlatformObject()->As<dx12::AccelerationStructure::PlatObj*>();

		SEAssert(platObj->m_ASBuffer, "BLAS buffer is null. This should not be possible");
		SEAssert(!doUpdate || platObj->m_isBuilt, "Can't update a BLAS that has not been created");

		re::AccelerationStructure::BLASParams const* blasParams =
			dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas.GetASParams());
		SEAssert(blasParams, "Failed to get BLASParams");

		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
		geometryDescs.reserve(blasParams->m_geometry.size());

		for (size_t instanceIdx = 0; instanceIdx < blasParams->m_geometry.size(); ++instanceIdx)
		{
			re::AccelerationStructure::Geometry const& geo = blasParams->m_geometry[instanceIdx];

			// Transform:
			D3D12_GPU_VIRTUAL_ADDRESS transform3x4Addr = NULL;
			if (blasParams->m_transform)
			{
				transform3x4Addr = dx12::Buffer::GetGPUVirtualAddress(blasParams->m_transform.get());

				constexpr uint32_t transformByteSize = 4 * 3 * 4; // 4B float x 3x4 elements
				transform3x4Addr += static_cast<uint64_t>(instanceIdx) * transformByteSize;

				SEAssert(transform3x4Addr % D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT == 0,
					"Transform addresses must be aligned to 16 bytes");
			}

			// Indices:
			DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
			uint32_t indexCount = 0;
			D3D12_GPU_VIRTUAL_ADDRESS indexBufferAddr = NULL;
			if (geo.GetVertexIndices())
			{
				indexFormat = dx12::DataTypeToDXGI_FORMAT(geo.GetVertexIndices()->GetDataType(), false);
				SEAssert(indexFormat == DXGI_FORMAT_UNKNOWN ||
					indexFormat == DXGI_FORMAT_R32_UINT ||
					indexFormat == DXGI_FORMAT_R16_UINT,
					"Invalid index format");

				indexCount = geo.GetVertexIndices()->GetNumElements();
				indexBufferAddr = dx12::Buffer::GetGPUVirtualAddress(geo.GetVertexIndices()->GetBuffer());
			}

			// Positions:
			const DXGI_FORMAT vertexFormat = dx12::DataTypeToDXGI_FORMAT(geo.GetVertexPositions().GetStream()->GetDataType(), false);
			const uint32_t vertexCount = geo.GetVertexPositions().GetStream()->GetNumElements();
			const D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE positionBufferAddr = D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE{
				.StartAddress = dx12::Buffer::GetGPUVirtualAddress(geo.GetVertexPositions().GetBuffer()),
				.StrideInBytes = re::DataTypeToByteStride(geo.GetVertexPositions().GetStream()->GetDataType()),};

			geometryDescs.emplace_back(D3D12_RAYTRACING_GEOMETRY_DESC{
				.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
				.Flags = GeometryFlagsToD3DGeometryFlags(geo.GetGeometryFlags()),
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
		SEAssert(geometryDescs.size() < 0xFFFFFF, "Beyond D3D12 maximum no. geometries in a BLAS");

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = BuildFlagsToD3DBuildFlags(blasParams->m_buildFlags);
		if (doUpdate)
		{
			SEAssert(blasParams->m_buildFlags & re::AccelerationStructure::BuildFlags::AllowUpdate,
				"Trying to update a BLAS, but the build flags don't have the AllowUpdate bit set");

			// Note: We must add the "perform update" flag to the exact same flags we used to create our original buffer,
			// or else we'll get the wrong buffer sizes
			flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}
		else
		{
			platObj->m_isBuilt = true;
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
		ComputeASBufferSizes(blasInputs, platObj->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);

		// Create a temporary scratch buffer:	
		// Note: We allow the scratch buffer to immediately go out of scope and rely on the the dx12::HeapManager 
		// deferred deletion to guarantee  lifetime
		const uint64_t scratchBufferSize = doUpdate ? updateScratchDataByteSize : scratchDataByteSize;

		std::unique_ptr<dx12::GPUResource> scratchBuffer = platObj->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					scratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_COMMON, },
			L"BuildBLAS temporary scratch buffer");

		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc{
			.DestAccelerationStructureData = platObj->m_ASBuffer->GetGPUVirtualAddress(),
			.Inputs = blasInputs,
			.SourceAccelerationStructureData = doUpdate ? platObj->m_ASBuffer->GetGPUVirtualAddress() : NULL,
			.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress(),
		};

		// Finally, record the work:
		cmdList->BuildRaytracingAccelerationStructure(
			&blasDesc,
			0,			// NumPostbuildInfoDescs
			nullptr);	// pPostbuildInfoDescs
	}


	void CreateTLASResources(re::AccelerationStructure& tlas)
	{
		SEAssert(tlas.GetType() == re::AccelerationStructure::Type::TLAS, "Invalid type");

		dx12::AccelerationStructure::PlatObj* platObj =
			tlas.GetPlatformObject()->As<dx12::AccelerationStructure::PlatObj*>();

		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas.GetASParams());
		SEAssert(tlasParams, "Failed to get TLASParams");

		// Compute the estimated buffer sizes:
		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{
			.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
			.Flags = BuildFlagsToD3DBuildFlags(tlasParams->m_buildFlags),
			.NumDescs = util::CheckedCast<uint32_t>(tlasParams->GetBLASInstances().size()),
			.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		};
		uint64_t resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize;
		ComputeASBufferSizes(tlasInputs, platObj->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);

		// Create the TLAS buffer:
		platObj->m_ASBuffer = platObj->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					resultDataMaxByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, },
			util::ToWideString(tlas.GetName()).c_str());

		// Create an SRV to describe the TLAS:
		dx12::Context* context = re::RenderManager::Get()->GetContext()->As<dx12::Context*>();
		platObj->m_tlasSRV = context->GetCPUDescriptorHeapMgr(dx12::CPUDescriptorHeapManager::CBV_SRV_UAV).Allocate(1);
		
		const D3D12_SHADER_RESOURCE_VIEW_DESC tlasSRVDesc{
			.Format = DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.RaytracingAccelerationStructure = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV{
				.Location = platObj->m_ASBuffer->GetGPUVirtualAddress() },
		};
		
		context->GetDevice().GetD3DDevice()->CreateShaderResourceView(
			nullptr, // null as the resource location is passed via the D3D12_SHADER_RESOURCE_VIEW_DESC
			&tlasSRVDesc,
			platObj->m_tlasSRV.GetBaseDescriptor());
	}


	void BuildTLAS(re::AccelerationStructure& tlas, bool doUpdate, ID3D12GraphicsCommandList4* cmdList)
	{
		SEAssert(tlas.GetType() == re::AccelerationStructure::Type::TLAS, "Invalid type");

		dx12::AccelerationStructure::PlatObj* platObj =
			tlas.GetPlatformObject()->As<dx12::AccelerationStructure::PlatObj*>();

		SEAssert(platObj->m_ASBuffer, "BLAS buffer is null. This should not be possible");
		SEAssert(!doUpdate || platObj->m_isBuilt, "Can't update a BLAS that has not been created");

		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas.GetASParams());
		SEAssert(tlasParams, "Failed to get TLASParams");

		// Compute the hit group indexes:
		auto ComputeStyleHash = [](std::vector<re::AccelerationStructure::Geometry> const& geometry)
			-> util::HashKey
			{
				// We only want to include an EffectID/Material Drawstyle bit combination once in our hash (i.e. the
				// style hash must be invariant to the number of geometry entries in a BLAS instance)
				std::set<util::HashKey> uniqueEffectIDsAndDrawstyleBits;
			
				util::HashKey styleHash;
				for (auto const& geo : geometry)
				{
					// We don't have knowledge of what shaders will eventually be resolved, as we don't have the hit
					// group drawstyle bits that will be passed to our ShaderBindingTable. However, these drawstyle bits
					// are identical for all hit groups, thus we can use the geometry EffectID and material drawstyle
					// bits to differentiate BLAS instances that will eventually resolve to a specific hit group shader					
					util::HashKey curHash;
					util::AddDataToHash(curHash, geo.GetEffectID());
					util::AddDataToHash(curHash, geo.GetDrawstyleBits());

					if (!uniqueEffectIDsAndDrawstyleBits.contains(curHash))
					{
						util::AddDataToHash(styleHash, curHash);
						uniqueEffectIDsAndDrawstyleBits.emplace(curHash);
					}
				}
				return styleHash;
			};

		uint32_t currentHitGroupIdx = 0;
		std::map<util::HashKey, uint32_t> styleHashToHitGroupIdx;
		for (auto const& blas : tlasParams->GetBLASInstances())
		{
			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blas->GetASParams());
			SEAssert(blasParams, "Failed to get TLASParams");

			util::HashKey const& styleHash = ComputeStyleHash(blasParams->m_geometry);
			if (!styleHashToHitGroupIdx.contains(styleHash))
			{
				SEAssert(currentHitGroupIdx < 0xFFFFFF, "Hit group indexes have a maximum of 24 bits");

				styleHashToHitGroupIdx.emplace(styleHash, currentHitGroupIdx++);
			}
		}

		// Create a temporary TLAS instance descriptor buffer:	
		// Note: We allow this resource to immediately go out of scope and rely on the the dx12::HeapManager 
		// deferred deletion to guarantee lifetime
		const uint64_t instanceDescriptorsSize = ComputeTLASInstancesBufferSize(tlasParams);
		SEAssert(instanceDescriptorsSize > 0, "Invalid TLAS buffer size. Trying to build an empty TLAS?");

		std::unique_ptr<dx12::GPUResource> TLASInstanceDescs = platObj->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceDescriptorsSize, D3D12_RESOURCE_FLAG_NONE),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD,
				.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ, },
				util::ToWideString(tlas.GetName()).c_str());

		// Map our TLAS instance descriptor buffer:
		D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = nullptr;
		const HRESULT hr = TLASInstanceDescs->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));
		SEAssert(SUCCEEDED(hr), "Failed to map TLAS instance descriptions buffer");

		memset(instanceDescs, 0, instanceDescriptorsSize); // Zero-initialize the mapped instance descriptors

		// Copy in the instance descriptors:
		const uint32_t numInstances = util::CheckedCast<uint32_t>(tlasParams->GetBLASInstances().size());
		SEAssert(numInstances < 0xFFFFFF, "Beyond D3D12 maximum no. instances in a TLAS");

		uint32_t blasBaseOffset = 0;
		for (uint32_t blasInstanceIdx = 0; blasInstanceIdx < numInstances; ++blasInstanceIdx)
		{
			SEAssert(tlasParams->GetBLASInstances()[blasInstanceIdx]->GetType() == re::AccelerationStructure::Type::BLAS,
				"Invalid BLAS instance type");
			SEAssert(blasInstanceIdx <= 0xFFFFFF, "24 bit max instance IDs");

			std::shared_ptr<re::AccelerationStructure> const& blasAS = tlasParams->GetBLASInstances()[blasInstanceIdx];

			re::AccelerationStructure::BLASParams const* blasParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(blasAS->GetASParams());
			SEAssert(blasParams, "Failed to get BLASParams");

			util::HashKey const& styleHash = ComputeStyleHash(blasParams->m_geometry);
			const uint32_t instanceContributionToHitGroupIndex = styleHashToHitGroupIdx.at(styleHash);

			dx12::AccelerationStructure::PlatObj* blasPlatObj =
				blasAS->GetPlatformObject()->As<dx12::AccelerationStructure::PlatObj*>();

			SEAssert(blasPlatObj->m_ASBuffer->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT == 0,
				"Invalid AS GPU address");

			instanceDescs[blasInstanceIdx] = D3D12_RAYTRACING_INSTANCE_DESC{
				// .Transform set below
				.InstanceID = blasBaseOffset, // HLSL: InstanceID() -> Arbitrary identifier for each unique BLAS instance
				.InstanceMask = blasParams->m_instanceMask,
				.InstanceContributionToHitGroupIndex = instanceContributionToHitGroupIndex,
				.Flags = util::CheckedCast<uint32_t>(InstanceFlagsToD3DInstanceFlags(blasParams->m_instanceFlags)),
				.AccelerationStructure = blasPlatObj->m_ASBuffer->GetGPUVirtualAddress(),
			};
			SEStaticAssert(sizeof(blasParams->m_blasWorldMatrix) == sizeof(instanceDescs[blasInstanceIdx].Transform),
				"Matrix size mismatch");

			memcpy(&instanceDescs[blasInstanceIdx].Transform,
				&blasParams->m_blasWorldMatrix[0][0],
				sizeof(instanceDescs[blasInstanceIdx].Transform));

			// Offset by the number of geometry instances insde the BLAS: We'll use this to index into arrays aligned
			// according to BLAS geometry
			blasBaseOffset += util::CheckedCast<uint32_t>(blasParams->m_geometry.size());
		}
		TLASInstanceDescs->Unmap(0, nullptr);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = BuildFlagsToD3DBuildFlags(tlasParams->m_buildFlags);
		if (doUpdate)
		{
			SEAssert(tlasParams->m_buildFlags & re::AccelerationStructure::BuildFlags::AllowUpdate,
				"Trying to update a TLAS, but the build flags don't have the AllowUpdate bit set");

			flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}
		else
		{
			platObj->m_isBuilt = true;
		}

		SEAssert(TLASInstanceDescs->GetGPUVirtualAddress() % 16 == 0,
			"Invalid InstanceDescs alignment (D3D12_RAYTRACING_INSTANCE_DESC_BYTE_ALIGNMENT)");

		// Compute the estimated buffer sizes:
		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{
			.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
			.Flags = flags,
			.NumDescs = util::CheckedCast<uint32_t>(tlasParams->GetBLASInstances().size()),
			.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
			.InstanceDescs = TLASInstanceDescs->GetGPUVirtualAddress(),
		};
		uint64_t resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize;
		ComputeASBufferSizes(tlasInputs, platObj->m_device, resultDataMaxByteSize, scratchDataByteSize, updateScratchDataByteSize);

		// Create a temporary scratch buffer:	
		// Note: We allow the scratch buffer to immediately go out of scope and rely on the the dx12::HeapManager 
		// deferred deletion to guarantee  lifetime
		const uint64_t scratchBufferSize = doUpdate ? updateScratchDataByteSize : scratchDataByteSize;

		std::unique_ptr<dx12::GPUResource> scratchBuffer = platObj->m_heapManager->CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
					scratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				.m_heapType = D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT,
				.m_initialState = D3D12_RESOURCE_STATE_COMMON, },
			L"BuildTLAS temporary scratch buffer");
		
		SEAssert(platObj->m_ASBuffer->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT == 0,
			"Invalid AS GPU address");
		SEAssert(scratchBuffer->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT == 0,
			"Invalid scratch AS GPU address");

		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc {
			.DestAccelerationStructureData = platObj->m_ASBuffer->GetGPUVirtualAddress(),
			.Inputs = tlasInputs,
			.SourceAccelerationStructureData = doUpdate ? platObj->m_ASBuffer->GetGPUVirtualAddress() : 0,
			.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress(),
		};

		// Finally, record the work:
		cmdList->BuildRaytracingAccelerationStructure(
			&tlasDesc,
			0,			// NumPostbuildInfoDescs
			nullptr);	// pPostbuildInfoDescs
	}
}

namespace dx12
{
	AccelerationStructure::PlatObj::PlatObj()
	{
		dx12::Context* context = re::RenderManager::Get()->GetContext()->As<dx12::Context*>();

		m_heapManager = &context->GetHeapManager();
		
		ComPtr<ID3D12Device5> device5;
		CheckHResult(context->GetDevice().GetD3DDevice().As(&device5), "Failed to get device5");

		m_device = device5.Get();
	}


	void AccelerationStructure::PlatObj::Destroy()
	{
		m_heapManager = nullptr;
		m_device = nullptr;
		m_ASBuffer = nullptr;
		m_isBuilt = false;
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
			CreateTLASResources(as);
		}
		break;
		case re::AccelerationStructure::Type::BLAS:
		{
			CreateBLASResources(as);
		}
		break;
		default: SEAssertF("Invalid AccelerationStructure Type");
		}
	}


	void AccelerationStructure::Destroy(re::AccelerationStructure& as)
	{
		//
	}


	void AccelerationStructure::BuildAccelerationStructure(
		re::AccelerationStructure& as,
		bool doUpdate,
		ID3D12GraphicsCommandList4* cmdList)
	{
		// Note: We assume all resource state transitions have already been recorded

		switch (as.GetType())
		{
		case re::AccelerationStructure::Type::TLAS:
		{
			BuildTLAS(as, doUpdate, cmdList);
		}
		break;
		case re::AccelerationStructure::Type::BLAS:
		{
			BuildBLAS(as, doUpdate, cmdList);
		}
		break;
		default: SEAssertF("Invalid AccelerationStructure Type");
		}
	}
}