// © 2025 Adam Badke. All rights reserved.
#include "AccelerationStructure_DX12.h"
#include "Buffer_DX12.h"
#include "BufferView.h"
#include "CommandList_DX12.h"
#include "Context_DX12.h"
#include "GPUDescriptorHeap_DX12.h"
#include "Shader_DX12.h"
#include "ShaderBindingTable_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture_DX12.h"
#include "TextureView.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>

using Microsoft::WRL::ComPtr;


namespace
{
	struct LibraryDesc
	{
		// We (currently) compile each shader type into its own DXIL library (with multiple libraries per re::Shader)
		ID3DBlob* m_dxilLibraryBlob = nullptr;
		std::wstring m_entryPointWStr; // So we can pass a wchar_t* to D3D
		D3D12_EXPORT_DESC m_exportDesc;
		D3D12_DXIL_LIBRARY_DESC m_dxilLibraryDesc;
	};
	std::vector<LibraryDesc> BuildLibraryDescriptions(
		std::ranges::range auto&& rayGenShaders,
		std::ranges::range auto&& missShaders,
		std::ranges::range auto&& hitGroupShaders)
	{
		auto CountLibraryDescriptions = [](std::ranges::range auto&& shaders) -> uint32_t
			{
				uint32_t numLibraryDescriptions = 0;
				for (auto const& shader : shaders)
				{
					std::vector<re::Shader::Metadata> const& metadata = shader->GetMetadata();

					numLibraryDescriptions += util::CheckedCast<uint32_t>(shader->GetMetadata().size());
				}
				return numLibraryDescriptions;
			};

		auto AppendLibraryDescriptions = [](std::ranges::range auto&& shaders, std::vector<LibraryDesc>& libraryDescs)
			{
				for (auto const& shader : shaders)
				{
					dx12::Shader::PlatformParams const* shaderPlatParams =
						shader->GetPlatformParams()->As<dx12::Shader::PlatformParams const*>();

					std::vector<re::Shader::Metadata> const& metadata = shader->GetMetadata();
					for (auto const& entry : metadata)
					{
						const re::Shader::ShaderType shaderType = entry.m_type;
						SEAssert(re::Shader::IsRayTracingType(shaderType), "Invalid shader type");
						SEAssert(shaderPlatParams->m_shaderBlobs[shaderType], "Missing DXIL blob for shader type");

						LibraryDesc& libDesc = libraryDescs.emplace_back(LibraryDesc{});

						libDesc.m_dxilLibraryBlob = shaderPlatParams->m_shaderBlobs[shaderType].Get();

						libDesc.m_entryPointWStr = util::ToWideString(entry.m_entryPoint); // So we can pass a wchar_t* to D3D

						libDesc.m_exportDesc = D3D12_EXPORT_DESC{
							.Name = libDesc.m_entryPointWStr.c_str(),
							.ExportToRename = nullptr,
							.Flags = D3D12_EXPORT_FLAG_NONE,
						};

						libDesc.m_dxilLibraryDesc = D3D12_DXIL_LIBRARY_DESC{
							.DXILLibrary = D3D12_SHADER_BYTECODE{
								.pShaderBytecode = shaderPlatParams->m_shaderBlobs[shaderType]->GetBufferPointer(),
								.BytecodeLength = shaderPlatParams->m_shaderBlobs[shaderType]->GetBufferSize(),
							},
							.NumExports = 1,
							.pExports = &libDesc.m_exportDesc,
						};
					}
				}
			};

		// Note: We must pre-reserve the correct vector size to prevent re-allocation, as library descriptions contain
		// pointers to other library descriptions
		const uint32_t numLibraryDescriptions =
			CountLibraryDescriptions(rayGenShaders) +
			CountLibraryDescriptions(missShaders) +
			CountLibraryDescriptions(hitGroupShaders);

		std::vector<LibraryDesc> libraryDescs;
		libraryDescs.reserve(numLibraryDescriptions);

		AppendLibraryDescriptions(rayGenShaders, libraryDescs);
		AppendLibraryDescriptions(missShaders, libraryDescs);
		AppendLibraryDescriptions(hitGroupShaders, libraryDescs);

		SEAssert(libraryDescs.size() == numLibraryDescriptions, "Unexpected library descriptions size");

		return libraryDescs;
	}


	// Return a list of export symbols: Ray gen. entry point name, miss shader entry point names, & hit group names
	struct ShaderExportSymbols
	{
		std::vector<std::wstring> m_symbolNames;
		std::vector<wchar_t const*> m_symbolPtrs;
	};
	ShaderExportSymbols BuildShaderExportSymbolsList(
		std::vector<core::InvPtr<re::Shader>> const& rayGenShaders,
		std::vector<core::InvPtr<re::Shader>> const& missShaders,
		std::vector<std::pair<std::string, core::InvPtr<re::Shader>>> const& hitGroupShaders)
	{
		const size_t estimatedSize = rayGenShaders.size() + missShaders.size() + (hitGroupShaders.size() * 2);

		ShaderExportSymbols exports;
		exports.m_symbolNames.reserve(estimatedSize);
		exports.m_symbolPtrs.reserve(estimatedSize);

		auto AddShaderEntryPointNames = [&exports](std::vector<core::InvPtr<re::Shader>> const& shaders)
			{
				for (auto const& shader : shaders)
				{
					for (auto const& metadata : shader->GetMetadata())
					{
						exports.m_symbolNames.emplace_back(util::ToWideString(metadata.m_entryPoint));
						exports.m_symbolPtrs.emplace_back(exports.m_symbolNames.back().c_str());
					}					
				}
			};
		AddShaderEntryPointNames(rayGenShaders);
		AddShaderEntryPointNames(missShaders);

		// Process hit shaders: We store the hit shader group name (i.e. the Technique name) in our pair.first:
		for (auto const& hitGroup : hitGroupShaders)
		{
			exports.m_symbolNames.emplace_back(util::ToWideString(hitGroup.first));
			exports.m_symbolPtrs.emplace_back(exports.m_symbolNames.back().c_str());
		}

		SEAssert(exports.m_symbolNames.size() <= estimatedSize && 
			exports.m_symbolPtrs.size() <= estimatedSize,
			"Number of export elements is larger than expected");

		return exports;
	}


	struct HitGroupDesc
	{
		// We need to pass wchar_t* names to D3D, this allows us to guarantee they're in scope
		std::wstring m_hitGroupName;
		std::wstring m_closestHitEntryPoint;
		std::wstring m_anyHitEntryPoint;
		std::wstring m_intersectionEntryPoint;
		D3D12_HIT_GROUP_DESC m_hitGroupDesc;
	};
	std::vector<HitGroupDesc> BuildHitGroupDescs(
		std::vector<std::pair<std::string, core::InvPtr<re::Shader>>> const& hitGroupNamesAndShaders)
	{
		std::vector<HitGroupDesc> hitGroupDescs;
		hitGroupDescs.reserve(hitGroupNamesAndShaders.size());

		for (auto const& entry : hitGroupNamesAndShaders)
		{
			core::InvPtr<re::Shader> const& shader = entry.second;
			std::vector<re::Shader::Metadata> const& metadata = shader->GetMetadata();
			SEAssert(!metadata.empty(), "Shader metadata is empty");

			HitGroupDesc& hitGroupDesc = hitGroupDescs.emplace_back();

			hitGroupDesc.m_hitGroupName = util::ToWideString(entry.first);

			D3D12_HIT_GROUP_TYPE hitGroupType = D3D12_HIT_GROUP_TYPE_TRIANGLES;

			for (auto const& entry : metadata)
			{
				const re::Shader::ShaderType shaderType = entry.m_type;
				SEAssert(re::Shader::IsRayTracingHitGroupType(shaderType), "Invalid shader type");

				switch (shaderType)
				{
				case re::Shader::ShaderType::HitGroup_Intersection:
				{
					hitGroupDesc.m_intersectionEntryPoint = util::ToWideString(entry.m_entryPoint);
					hitGroupType = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
				}
				break;
				case re::Shader::ShaderType::HitGroup_AnyHit:
				{
					hitGroupDesc.m_anyHitEntryPoint = util::ToWideString(entry.m_entryPoint);
				}
				break;
				case re::Shader::ShaderType::HitGroup_ClosestHit:
				{
					hitGroupDesc.m_closestHitEntryPoint = util::ToWideString(entry.m_entryPoint);
				}
				break;
				default: SEAssertF("Invalid hit group shader type");
				}
			}

			// Populate our D3D object with our wchar_t* names:
			hitGroupDesc.m_hitGroupDesc = D3D12_HIT_GROUP_DESC{
				.HitGroupExport = hitGroupDesc.m_hitGroupName.c_str(),
				.Type = hitGroupType,
				.AnyHitShaderImport = hitGroupDesc.m_anyHitEntryPoint.empty() ? 
					nullptr : hitGroupDesc.m_anyHitEntryPoint.c_str(),
				.ClosestHitShaderImport = hitGroupDesc.m_closestHitEntryPoint.empty() ?
					nullptr : hitGroupDesc.m_closestHitEntryPoint.c_str(),
				.IntersectionShaderImport = hitGroupDesc.m_intersectionEntryPoint.empty() ?
					nullptr : hitGroupDesc.m_intersectionEntryPoint.c_str(),
			};
		}
		return hitGroupDescs;
	}


	struct RootSignatureAssociation
	{
		ID3D12RootSignature* m_rootSignature;
		std::vector<std::wstring> m_symbolNames;
		std::vector<wchar_t const*> m_symbolNamePtrs;

		// Note: We populate this once we've added a sub-object to associate the root signature and the exported shader
		// symbols (as it requires a pointer to it)
		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION m_exportsAssociation;
	};
	std::vector<RootSignatureAssociation> BuildRootSignatureAssociations(
		std::ranges::range auto&& rayGenShaders, 
		std::ranges::range auto&& missShaders, 
		std::ranges::range auto&& hitGroupShaders)
	{
		auto CountRootSignatureAssociations = [](std::ranges::range auto&& shaders) -> uint32_t
			{
				uint32_t count = 0;
				for (auto const& shader : shaders)
				{
					count += util::CheckedCast<uint32_t>(shader->GetMetadata().size());
				}
				return count;
			};

		auto AddRootSignatureAssociations = [](
			std::ranges::range auto&& shaders,
			std::vector<RootSignatureAssociation>& rootSigAssociations)
			{
				for (auto const& shader : shaders)
				{
					dx12::Shader::PlatformParams const* shaderPlatParams =
						shader->GetPlatformParams()->As<dx12::Shader::PlatformParams const*>();

					RootSignatureAssociation& rootSigAssociation = rootSigAssociations.emplace_back();

					rootSigAssociation.m_rootSignature = shaderPlatParams->m_rootSignature->GetD3DRootSignature();

					// Add the export symbol names:
					for (auto const& entry : shader->GetMetadata())
					{
						std::wstring const& symbolName =
							rootSigAssociation.m_symbolNames.emplace_back(util::ToWideString(entry.m_entryPoint));
						rootSigAssociation.m_symbolNamePtrs.emplace_back(symbolName.c_str());
					}
				}
			};
		
		std::vector<RootSignatureAssociation> rootSigAssociations;
		rootSigAssociations.reserve(
			CountRootSignatureAssociations(rayGenShaders) + 
			CountRootSignatureAssociations(missShaders) + 
			CountRootSignatureAssociations(hitGroupShaders));

		AddRootSignatureAssociations(rayGenShaders, rootSigAssociations);
		AddRootSignatureAssociations(missShaders, rootSigAssociations);
		AddRootSignatureAssociations(hitGroupShaders, rootSigAssociations);

		return rootSigAssociations;
	}


	void CreateD3DStateObject(
		re::ShaderBindingTable& sbt,
		std::vector<core::InvPtr<re::Shader>> const& rayGenShaders, 
		std::vector<core::InvPtr<re::Shader>> const& missShaders, 
		std::vector<std::pair<std::string, core::InvPtr<re::Shader>>> const& hitGroupShaders)
	{
		dx12::ShaderBindingTable::PlatformParams* sbtPlatParams = 
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams*>();

		SEAssert(sbtPlatParams->m_rayTracingStateObject == nullptr, 
			"State object already exists. Releasing now may destroy the resource while it is still in use");

		// Hit group shaders are paired with the hit group name, a view isolates the InvPtr<Shader> for convenience
		auto hitGroupShaderView = hitGroupShaders | std::views::transform(
			[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
			{
				return hitGroupNamesAndShader.second;
			});


		// Populate an array of RT state sub-objects:
		// Note: Sub-objects may contain pointers to other sub-objects in this array. For now, we just reserve the
		// vector to prevent re-allocation during population and assert the size didn't change, but it would be better
		// to count the number of sub-allocations required in advance
		constexpr size_t k_expectedNumSubObjects = 128;
		std::vector<D3D12_STATE_SUBOBJECT> subObjects;
		subObjects.reserve(k_expectedNumSubObjects);
	

		// Build a list of library descriptions (this guarantees the various pointers D3D requires are in scope)
		std::vector<LibraryDesc> const& libraryDescs = 
			BuildLibraryDescriptions(rayGenShaders, missShaders, hitGroupShaderView);

		// Add the DXIL library description state sub-objects:
		for (auto const& libDesc : libraryDescs)
		{
			subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
				.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
				.pDesc = &libDesc.m_dxilLibraryDesc,
				});
		}


		// Hit group declarations:
		bool hasIntersectionShader = false;
		std::vector<HitGroupDesc> const& hitGroupDescs = BuildHitGroupDescs(hitGroupShaders);
		for (auto const& hitGroup : hitGroupDescs)
		{
			subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
				.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
				.pDesc = &hitGroup.m_hitGroupDesc,
				});

			SEAssert(hitGroup.m_hitGroupDesc.Type != D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE || 
				!hitGroup.m_intersectionEntryPoint.empty(),
				"Found a hit group for procedural primitives that does not have an intersection shader entry point");

			hasIntersectionShader |= (hitGroup.m_intersectionEntryPoint.empty() == false);
		}


		re::ShaderBindingTable::SBTParams const& sbtParams = sbt.GetSBTParams();

		// Shader payload configuration:
		const D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {
			.MaxPayloadSizeInBytes = sbtParams.m_maxPayloadByteSize,
			.MaxAttributeSizeInBytes = 2 * sizeof(float), // sizeof HLSL's BuiltInTriangleIntersectionAttributes (i.e. Barycentrics)
		};
		SEAssert(!hasIntersectionShader, "TODO: Handle MaxAttributeSizeInBytes for intersection shaders");

		D3D12_STATE_SUBOBJECT const& shaderConfigSubObject = subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
			.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
			.pDesc = &shaderConfig,
		});


		// Build a list of symbol names for ray gen shaders, miss shaders, and hit group names so we can link them to
		// the payload definition:
		ShaderExportSymbols shaderExportSymbols = 
			BuildShaderExportSymbolsList(rayGenShaders, missShaders, hitGroupShaders);

		const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION exportsAssociation{
			.pSubobjectToAssociate = &shaderConfigSubObject,
			.NumExports = util::CheckedCast<uint32_t>(shaderExportSymbols.m_symbolPtrs.size()),
			.pExports = shaderExportSymbols.m_symbolPtrs.data(),
		};

		subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
			.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
			.pDesc = &exportsAssociation,
			});


		// Root signature associations:
		std::vector<RootSignatureAssociation> rootSigAssociations = 
			BuildRootSignatureAssociations(rayGenShaders, missShaders, hitGroupShaderView);

		for (auto& association : rootSigAssociations)
		{
			// Add a sub-object to declare the root signature:
			D3D12_STATE_SUBOBJECT& rootSigDeclaration = subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
				.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
				.pDesc = &association.m_rootSignature,
				});

			// Now we can populate the association's D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION m_exportsAssociation:
			association.m_exportsAssociation.pSubobjectToAssociate = &rootSigDeclaration;
			association.m_exportsAssociation.NumExports = 
				util::CheckedCast<uint32_t>(association.m_symbolNamePtrs.size());
			association.m_exportsAssociation.pExports = association.m_symbolNamePtrs.data();

			// Add a sub-object for the association between the exported shader symbols and the root signature:
			subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
				.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
				.pDesc = &association.m_exportsAssociation,
				});
		}

		// Ray tracing pipeline configuration:
		const D3D12_RAYTRACING_PIPELINE_CONFIG rtPipelineConfig{
			.MaxTraceRecursionDepth = sbtParams.m_maxRecursionDepth,
		};
		subObjects.emplace_back(D3D12_STATE_SUBOBJECT{
			.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
			.pDesc = &rtPipelineConfig,
			});


		SEAssert(subObjects.size() < k_expectedNumSubObjects,
			"More sub-objects than expected were allocated - the subObjects vector likely re-allocated and invalidated "
			"pointers between elements. Increase k_expectedNumSubObjects, or pre-count the number of sub-objects in advance");

		// Ray tracing pipeline state object:
		const D3D12_STATE_OBJECT_DESC stateObjectDesc{
			.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
			.NumSubobjects = util::CheckedCast<uint32_t>(subObjects.size()),
			.pSubobjects = subObjects.data(),
		};

		// Finally, create our ray tracing state object and query interface:
		ComPtr<ID3D12Device5> device5;
		dx12::CheckHResult(
			re::Context::GetAs<dx12::Context*>()->GetDevice().GetD3DDevice().As(&device5),
			"Failed to get device5");

		dx12::CheckHResult(
			device5->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&sbtPlatParams->m_rayTracingStateObject)),
			"Failed to create ray tracing state object");

		dx12::CheckHResult(sbtPlatParams->m_rayTracingStateObject->QueryInterface(
			IID_PPV_ARGS(&sbtPlatParams->m_rayTracingStateObjectProperties)),
			"Failed to create the ray tracing state object properties query interface");
	}


	// Computes the entry stride - i.e. the maximum number of bytes of a single entry within a set of SBT entries - from
	// the number of parameters required by any of the given root signatures
	uint32_t ComputeIndividualEntrySize(std::ranges::range auto&& shaders)
	{
		// Find the maximum number of root signature parameters in the given set of shaders:
		uint32_t maxParams = 0;
		for (auto const& shader : shaders)
		{
			dx12::Shader::PlatformParams const* shaderPlatParams =
				shader->GetPlatformParams()->As<dx12::Shader::PlatformParams const*>();

			maxParams = std::max(maxParams, shaderPlatParams->m_rootSignature->GetNumRootSignatureEntries());
		}

		constexpr uint8_t k_entrySize = 8; // Each parameter in a SBT entry requires 8B

		uint32_t entryByteSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT + // Program ID size (e.g. 32B)
			(k_entrySize * maxParams);

		// Round up to maintain alignment for the rest of the table
		entryByteSize = 
			util::RoundUpToNearestMultiple<uint32_t>(entryByteSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		SEAssert(entryByteSize <= 4096, "Maximum shader region stride is 4096B with a 32B alignment");

		return entryByteSize;
	}


	// Returns the total region size (i.e. stride * no. of entries)
	uint32_t InitializeShaderRegions(
		ID3D12StateObjectProperties* raytracingPipeline,
		uint8_t* const mappedData,
		uint32_t stride,
		std::ranges::range auto&& shaders,
		std::ranges::range auto&& exportNames)
	{
		uint32_t numEntriesWritten = 0;

		for (size_t i = 0; i < shaders.size(); ++i)
		{
			auto const& shader = shaders[i];

			std::vector<re::Shader::Metadata> const& shaderMetadata = shader->GetMetadata();
			for (auto const& entry : shaderMetadata)
			{
				void* shaderIdentifier = 
					raytracingPipeline->GetShaderIdentifier(util::ToWideString(exportNames[i]).c_str());
				SEAssert(shaderIdentifier,
					"Failed to get a shader identifier for \"%s::%s\"",
					shader->GetName().c_str(), entry.m_entryPoint.c_str());

				// Compute the starting offset for the current shader entry:
				uint8_t* dst = mappedData + (i * stride);

				// Start by zero-initializing the region:
				memset(dst, 0, stride);

				// Copy the shader identifier to the beginning of the region:
				memcpy(dst, shaderIdentifier, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

				numEntriesWritten++;
			}
		}

		// Return the total bytes written for all entries (i.e. the offset for any subsquent writes):
		return numEntriesWritten * stride;
	}
}

namespace dx12
{
	void ShaderBindingTable::PlatformParams::Destroy()
	{
		m_SBT = nullptr;

		m_rayTracingStateObject = nullptr;
		m_rayTracingStateObjectProperties = nullptr;
	}


	// ---


	void ShaderBindingTable::Update(re::ShaderBindingTable& sbt, uint64_t currentFrameNum)
	{
		// Create the D3D state object:
		CreateD3DStateObject(sbt, sbt.m_rayGenShaders, sbt.m_missShaders, sbt.m_hitGroupNamesAndShaders);

		dx12::ShaderBindingTable::PlatformParams* platParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams*>();

		auto GetEntryPointTransform = std::views::transform([](auto const& shader) -> std::string const&
			{
				SEAssert(shader->GetMetadata().size() == 1, "More Metadata than expected");
				return shader->GetMetadata()[0].m_entryPoint;
			});

		auto rayGenExportNameView = sbt.m_rayGenShaders | GetEntryPointTransform;
		auto missExportNameView = sbt.m_missShaders | GetEntryPointTransform;
		auto callableExportNameView = sbt.m_callableShaders | GetEntryPointTransform;

		// Hit group shaders are paired with the hit group name, a view isolates the InvPtr<Shader> for convenience
		auto hitGroupShaderView = sbt.m_hitGroupNamesAndShaders | std::views::transform(
			[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
			{
				return hitGroupNamesAndShader.second;
			});

		auto hitGroupExportNameView = sbt.m_hitGroupNamesAndShaders | std::views::transform(
			[](auto const& hitGroupNamesAndShader) -> std::string const&
			{
				return hitGroupNamesAndShader.first;
			});


		// Compute the region size for each type of shader:
		platParams->m_rayGenRegionByteStride = ComputeIndividualEntrySize(sbt.m_rayGenShaders);
		platParams->m_missRegionByteStride = ComputeIndividualEntrySize(sbt.m_missShaders);
		platParams->m_hitGroupRegionByteStride = ComputeIndividualEntrySize(hitGroupShaderView);
		platParams->m_callableRegionTotalByteSize = ComputeIndividualEntrySize(sbt.m_callableShaders);

		// Compute the total SBT size for N frames-in-flight-worth of data
		// Note: We round size up to a multiple of 256B, as per the NVidia DXR sample
		platParams->m_numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();
		
		platParams->m_frameRegionByteSize = util::RoundUpToNearestMultiple(
			static_cast<uint64_t>(platParams->m_rayGenRegionByteStride) +
			static_cast<uint64_t>(platParams->m_missRegionByteStride) +
			static_cast<uint64_t>(platParams->m_hitGroupRegionByteStride) +
			static_cast<uint64_t>(platParams->m_callableRegionTotalByteSize),
			256llu);

		const uint64_t totalSBTByteSize = platParams->m_numFramesInFlight * platParams->m_frameRegionByteSize;

		// We rely on the HeapManager's deferred delete to guarantee the lifetime of any previous SBT buffer
		dx12::HeapManager& heapMgr = re::Context::GetAs<dx12::Context*>()->GetHeapManager();
		platParams->m_SBT = heapMgr.CreateResource(
			dx12::ResourceDesc{
				.m_resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSBTByteSize),
				.m_heapType = D3D12_HEAP_TYPE_UPLOAD,
				.m_initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
			},
			sbt.GetWName().c_str());

		// Finally, pack the shader IDs into the SBT (and zero-initialize the remaining memory):
		uint8_t* sbtData = nullptr;
		CheckHResult(platParams->m_SBT->Map(0, nullptr, reinterpret_cast<void**>(&sbtData)), "Failed to map SBT buffer");
		uint8_t* const baseSBTData = sbtData;
		
		// Initialize the first frame's worth of data, and compute the relative offsets for the remaining data:
		uint32_t numBytesWritten = 0;

		// Ray gen:
		platParams->m_rayGenRegionBaseOffset = 0; // Ray gen shader is the first entry
		platParams->m_rayGenRegionTotalByteSize = 
			platParams->m_rayGenRegionByteStride * util::CheckedCast<uint32_t>(sbt.m_rayGenShaders.size());
		const uint32_t numRayGenBytesWritten = InitializeShaderRegions(
			platParams->m_rayTracingStateObjectProperties.Get(),
			sbtData, 
			platParams->m_rayGenRegionByteStride, 
			sbt.m_rayGenShaders,
			rayGenExportNameView);
		sbtData += numRayGenBytesWritten;
		numBytesWritten += numRayGenBytesWritten;

		// Miss:
		platParams->m_missRegionBaseOffset = numBytesWritten;
		platParams->m_missRegionTotalByteSize =
			platParams->m_missRegionByteStride * util::CheckedCast<uint32_t>(sbt.m_missShaders.size());
		const uint32_t numMissBytesWritten = InitializeShaderRegions(
			platParams->m_rayTracingStateObjectProperties.Get(),
			sbtData,
			platParams->m_missRegionByteStride,
			sbt.m_missShaders,
			missExportNameView);
		sbtData += numMissBytesWritten;
		numBytesWritten += numMissBytesWritten;

		// Hit groups:
		platParams->m_hitGroupRegionBaseOffset = numBytesWritten;
		platParams->m_hitGroupRegionTotalByteSize = platParams->m_hitGroupRegionByteStride * sbt.GetNumHitGroupShaders();
		const uint32_t numHitGroupBytesWritten = InitializeShaderRegions(
			platParams->m_rayTracingStateObjectProperties.Get(),
			sbtData,
			platParams->m_hitGroupRegionByteStride,
			hitGroupShaderView,
			hitGroupExportNameView);
		sbtData += numHitGroupBytesWritten;
		numBytesWritten += numHitGroupBytesWritten;

		// Callable:
		platParams->m_callableRegionBaseOffset = numBytesWritten;
		platParams->m_callableRegionTotalByteSize =
			platParams->m_callableRegionByteStride * util::CheckedCast<uint32_t>(sbt.m_callableShaders.size());
		const uint32_t numCallableBytesWritten = InitializeShaderRegions(
			platParams->m_rayTracingStateObjectProperties.Get(),
			sbtData,
			platParams->m_callableRegionByteStride,
			sbt.m_callableShaders,
			callableExportNameView);
		sbtData += numCallableBytesWritten;
		numBytesWritten += numCallableBytesWritten;

		// Initialize the remaining frame data:
		for (uint8_t curFrameIdx = 1; curFrameIdx < platParams->m_numFramesInFlight; ++curFrameIdx)
		{
			// Re-set the sbtData pointer to the base of the next region:
			sbtData = baseSBTData + (curFrameIdx * platParams->m_frameRegionByteSize);

			const uint32_t numExtraRayGenBytesWritten = InitializeShaderRegions(
				platParams->m_rayTracingStateObjectProperties.Get(),
				sbtData,
				platParams->m_rayGenRegionByteStride,
				sbt.m_rayGenShaders,
				rayGenExportNameView);
			sbtData += numExtraRayGenBytesWritten;

			const uint32_t numExtraMissBytesWritten = InitializeShaderRegions(
				platParams->m_rayTracingStateObjectProperties.Get(),
				sbtData,
				platParams->m_missRegionByteStride,
				sbt.m_missShaders,
				missExportNameView);
			sbtData += numExtraMissBytesWritten;

			const uint32_t numExtraHitGroupBytesWritten = InitializeShaderRegions(
				platParams->m_rayTracingStateObjectProperties.Get(),
				sbtData,
				platParams->m_hitGroupRegionByteStride,
				hitGroupShaderView,
				hitGroupExportNameView);
			sbtData += numExtraHitGroupBytesWritten;

			const uint32_t numExtraCallableBytesWritten = InitializeShaderRegions(
				platParams->m_rayTracingStateObjectProperties.Get(),
				sbtData,
				platParams->m_callableRegionByteStride,
				sbt.m_callableShaders,
				callableExportNameView);
			sbtData += numExtraCallableBytesWritten;
		}

		// Cleanup:
		platParams->m_SBT->Unmap(0, nullptr);
	}


	// Helper to reduce boilerplate:
	static void WriteSBTElement(
		GPUResource* sbtGPUResource,
		std::function<void(uint8_t* dst, uint8_t dstByteSize, dx12::RootSignature::RootParameter const*)>&& SetData,
		std::string const& shaderName,
		std::ranges::range auto&& shaders,
		uint32_t regionBaseOffset,	// Base offset for start of shader region (e.g. rayGen/miss/hit groups)
		uint32_t regionByteStride,	// Element stride within the shader region
		uint64_t frameRegionByteSize,
		uint64_t currentFrameNum,
		uint8_t numFramesInFlight)
	{
		uint8_t* sbtData = nullptr; // We'll map this if necessary

		for (uint32_t i = 0; i < util::CheckedCast<uint32_t>(shaders.size()); ++i)
		{
			core::InvPtr<re::Shader> const& shader = shaders[i];

			dx12::Shader::PlatformParams const* shaderPlatParams =
				shader->GetPlatformParams()->As<dx12::Shader::PlatformParams const*>();

			dx12::RootSignature::RootParameter const* rootParam =
				shaderPlatParams->m_rootSignature->GetRootSignatureEntry(shaderName);
			if (rootParam)
			{
				// Map the SBT buffer:
				if (sbtData == nullptr)
				{
					CheckHResult(
						sbtGPUResource->Map(0, nullptr, reinterpret_cast<void**>(&sbtData)),
						"Failed to map SBT buffer");

					// Apply the per-frame base offset:
					sbtData += frameRegionByteSize * (currentFrameNum % numFramesInFlight);
				}

				const uint32_t regionOffset = regionBaseOffset + (i * regionByteStride);

				constexpr uint32_t k_baseOffset = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT; // Program ID is 1st element
				constexpr uint8_t k_entrySize = 8; // Each parameter in a SBT entry requires 8B

				uint8_t* dst = sbtData + regionOffset + k_baseOffset + (rootParam->m_index * k_entrySize);

				SetData(dst, k_entrySize, rootParam);
			}
		}

		// Cleanup:
		if (sbtData != nullptr)
		{
			sbtGPUResource->Unmap(0, nullptr);
		}
	}


	void ShaderBindingTable::SetTLASOnLocalRoots(
		re::ShaderBindingTable const& sbt,
		re::ASInput const& tlasInput,
		dx12::GPUDescriptorHeap* gpuDescHeap,
		uint64_t currentFrameNum)
	{
		dx12::ShaderBindingTable::PlatformParams const* sbtPlatParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();

		dx12::AccelerationStructure::PlatformParams const* tlasPlatParams =
			tlasInput.m_accelerationStructure->GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams const*>();

		auto SetData = [&tlasPlatParams, gpuDescHeap]
			(uint8_t* dst, uint8_t dstByteSize, dx12::RootSignature::RootParameter const* rootParam)
			{
				switch (rootParam->m_type)
				{
				case dx12::RootSignature::RootParameter::Type::Constant:
				case dx12::RootSignature::RootParameter::Type::CBV:
				case dx12::RootSignature::RootParameter::Type::UAV:
				{
					SEAssertF("Trying to set a TLAS to an unexpected root signature parameter type");
				}
				break;				
				case dx12::RootSignature::RootParameter::Type::SRV:
				{
					D3D12_GPU_VIRTUAL_ADDRESS const& tlasGPUVA = tlasPlatParams->m_ASBuffer->GetGPUVirtualAddress();
					memcpy(dst, &tlasGPUVA, dstByteSize);
				}
				break;
				case dx12::RootSignature::RootParameter::Type::DescriptorTable:
				{
					SEAssert(tlasPlatParams->m_tlasSRV.IsValid(), "TLAS SRV is not valid");
					D3D12_CPU_DESCRIPTOR_HANDLE const& tlasSRVHandle = tlasPlatParams->m_tlasSRV.GetBaseDescriptor();

					D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleTlasSRVHandle = 
						gpuDescHeap->CommitToGPUVisibleHeap({ tlasSRVHandle });

					memcpy(dst, &gpuVisibleTlasSRVHandle, dstByteSize);
				}
				break;
				default: SEAssertF("Invalid descriptor type");
				}
			};

		WriteSBTElement(
			sbtPlatParams->m_SBT.get(),
			SetData,
			tlasInput.m_shaderName,
			sbt.m_rayGenShaders, 
			sbtPlatParams->m_rayGenRegionBaseOffset, 
			sbtPlatParams->m_rayGenRegionByteStride,
			sbtPlatParams->m_frameRegionByteSize,
			currentFrameNum,
			sbtPlatParams->m_numFramesInFlight);

		WriteSBTElement(
			sbtPlatParams->m_SBT.get(),
			SetData,
			tlasInput.m_shaderName,
			sbt.m_missShaders,
			sbtPlatParams->m_missRegionBaseOffset,
			sbtPlatParams->m_missRegionByteStride,
			sbtPlatParams->m_frameRegionByteSize,
			currentFrameNum,
			sbtPlatParams->m_numFramesInFlight);

		WriteSBTElement(
			sbtPlatParams->m_SBT.get(),
			SetData,
			tlasInput.m_shaderName,
			sbt.m_hitGroupNamesAndShaders | std::views::transform(
				[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
				{
					return hitGroupNamesAndShader.second;
				}),
			sbtPlatParams->m_hitGroupRegionBaseOffset,
			sbtPlatParams->m_hitGroupRegionByteStride,
			sbtPlatParams->m_frameRegionByteSize,
			currentFrameNum,
			sbtPlatParams->m_numFramesInFlight);
	}


	void ShaderBindingTable::SetTexturesOnLocalRoots(
		re::ShaderBindingTable const& sbt,
		std::vector<re::TextureAndSamplerInput> const& texInputs,
		dx12::CommandList* cmdList,
		dx12::GPUDescriptorHeap* gpuDescHeap,
		uint64_t currentFrameNum)
	{
		dx12::ShaderBindingTable::PlatformParams const* sbtPlatParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();

		// Batch our resource transitions into a single call:
		std::vector<dx12::CommandList::TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(texInputs.size());

		for (auto const& texInput : texInputs)
		{
			auto SetData = [&texInput, &resourceTransitions, cmdList, gpuDescHeap]
				(uint8_t* dst, uint8_t dstByteSize, dx12::RootSignature::RootParameter const* rootParam)
				{
					SEAssert(rootParam->m_type == RootSignature::RootParameter::Type::DescriptorTable,
						"We currently assume all textures belong to descriptor tables");

					D3D12_CPU_DESCRIPTOR_HANDLE texDescriptor{};
					switch (rootParam->m_tableEntry.m_type)
					{
					case dx12::RootSignature::DescriptorType::SRV:
					{
						texDescriptor = dx12::Texture::GetSRV(texInput.m_texture, texInput.m_textureView);
					}
					break;
					case dx12::RootSignature::DescriptorType::UAV:
					{
						texDescriptor = dx12::Texture::GetUAV(texInput.m_texture, texInput.m_textureView);
					}
					break;
					default: SEAssertF("Invalid descriptor range type for a texture");
					}

					D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleTexDescriptor = 
						gpuDescHeap->CommitToGPUVisibleHeap({ texDescriptor });

					memcpy(dst, &gpuVisibleTexDescriptor, dstByteSize);

					// Record a resource transition:
					dx12::Texture::PlatformParams const* texPlatParams = 
						texInput.m_texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

					resourceTransitions.emplace_back(dx12::CommandList::TransitionMetadata{
							.m_resource = texPlatParams->m_gpuResource->Get(),
							.m_toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
							.m_subresourceIndexes = re::TextureView::GetSubresourceIndexes(
								texInput.m_texture, texInput.m_textureView),
						});
				};


			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				texInput.m_shaderName,
				sbt.m_rayGenShaders,
				sbtPlatParams->m_rayGenRegionBaseOffset,
				sbtPlatParams->m_rayGenRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				texInput.m_shaderName,
				sbt.m_missShaders,
				sbtPlatParams->m_missRegionBaseOffset,
				sbtPlatParams->m_missRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				texInput.m_shaderName,
				sbt.m_hitGroupNamesAndShaders | std::views::transform(
					[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
					{
						return hitGroupNamesAndShader.second;
					}),
				sbtPlatParams->m_hitGroupRegionBaseOffset,
				sbtPlatParams->m_hitGroupRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				texInput.m_shaderName,
				sbt.m_callableShaders,
				sbtPlatParams->m_callableRegionBaseOffset,
				sbtPlatParams->m_callableRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);
		}

		// Finally, record the resource transitions:
		cmdList->TransitionResources(std::move(resourceTransitions));
	}


	void ShaderBindingTable::SetBuffersOnLocalRoots(
		re::ShaderBindingTable const& sbt,
		std::vector<re::BufferInput> const& bufferInputs,
		dx12::CommandList* cmdList,
		dx12::GPUDescriptorHeap* gpuDescHeap,
		uint64_t currentFrameNum)
	{
		dx12::ShaderBindingTable::PlatformParams const* sbtPlatParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();

		// Batch our resource transitions into a single call:
		std::vector<dx12::CommandList::TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(bufferInputs.size());

		for (re::BufferInput const& bufferInput : bufferInputs)
		{
			re::Buffer const* buffer = bufferInput.GetBuffer();
			re::Buffer::BufferParams const& bufferParams = buffer->GetBufferParams();

			dx12::Buffer::PlatformParams* bufferPlatParams =
				buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();


			auto SetData = [&bufferInput, buffer, &bufferParams, bufferPlatParams, &resourceTransitions, cmdList, gpuDescHeap]
				(uint8_t* dst, uint8_t dstByteSize, dx12::RootSignature::RootParameter const* rootParam)
				{
					bool transitionResource = false;
					D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_COMMON; // Updated below

					// Don't transition resources representing shared heaps
					const bool isInSharedHeap = bufferParams.m_lifetime == re::Lifetime::SingleFrame;

					switch (rootParam->m_type)
					{
					case dx12::RootSignature::RootParameter::Type::Constant:
					{
						SEAssertF("Trying to set a Buffer to an unexpected root signature parameter type");
					}
					break;
					case dx12::RootSignature::RootParameter::Type::CBV:
					{
						const D3D12_GPU_VIRTUAL_ADDRESS bufferGPUVA =
							bufferPlatParams->m_resolvedGPUResource->GetGPUVirtualAddress() + bufferPlatParams->m_heapByteOffset;

						memcpy(dst, &bufferGPUVA, dstByteSize);

						toState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
						transitionResource = !isInSharedHeap;
					}
					break;
					case dx12::RootSignature::RootParameter::Type::SRV:
					{
						const D3D12_GPU_VIRTUAL_ADDRESS bufferGPUVA =
							bufferPlatParams->m_resolvedGPUResource->GetGPUVirtualAddress() + bufferPlatParams->m_heapByteOffset;

						memcpy(dst, &bufferGPUVA, dstByteSize);

						toState = (cmdList->GetCommandListType() == dx12::CommandListType::Compute ?
							D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

						transitionResource = !isInSharedHeap;
					}
					break;
					case dx12::RootSignature::RootParameter::Type::UAV:
					{
						const D3D12_GPU_VIRTUAL_ADDRESS bufferGPUVA =
							bufferPlatParams->m_resolvedGPUResource->GetGPUVirtualAddress() + bufferPlatParams->m_heapByteOffset;

						memcpy(dst, &bufferGPUVA, dstByteSize);

						toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						transitionResource = true;
					}
					break;
					case dx12::RootSignature::RootParameter::Type::DescriptorTable:
					{
						re::BufferView const& bufView = bufferInput.GetView();

						switch (rootParam->m_tableEntry.m_type)
						{
						case dx12::RootSignature::DescriptorType::SRV:
						{
							SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, bufferParams),
								"Buffer is missing the Structured usage bit");
							SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
								"SRV buffers must have GPU reads enabled");
							SEAssert(bufferPlatParams->m_heapByteOffset == 0, "Unexpected heap byte offset");

							D3D12_CPU_DESCRIPTOR_HANDLE const& bufferSRV = 
								dx12::Buffer::GetSRV(bufferInput.GetBuffer(), bufView);

							D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleSRV = 
								gpuDescHeap->CommitToGPUVisibleHeap({ bufferSRV });

							memcpy(dst, &gpuVisibleSRV, dstByteSize);

							toState = (cmdList->GetCommandListType() == dx12::CommandListType::Compute ?
								D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
							transitionResource = !isInSharedHeap;
						}
						break;
						case dx12::RootSignature::DescriptorType::UAV:
						{
							SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams),
								"Buffer is missing the Structured usage bit");
							SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
								"UAV buffers must have GPU writes enabled");
							SEAssert(bufferPlatParams->m_heapByteOffset == 0, "Unexpected heap byte offset");

							D3D12_CPU_DESCRIPTOR_HANDLE const& bufferUAV =
								dx12::Buffer::GetUAV(bufferInput.GetBuffer(), bufferInput.GetView());

							D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisiblUAV =
								gpuDescHeap->CommitToGPUVisibleHeap({ bufferUAV });

							memcpy(dst, &gpuVisiblUAV, dstByteSize);

							toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
							transitionResource = true;
						}
						break;
						case dx12::RootSignature::DescriptorType::CBV:
						{
							SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams),
								"Buffer is missing the Constant usage bit");
							SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
								!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
								"Invalid usage flags for a constant buffer");

							D3D12_CPU_DESCRIPTOR_HANDLE const& bufferCBV =
								dx12::Buffer::GetCBV(bufferInput.GetBuffer(), bufView);

							D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisiblCBV =
								gpuDescHeap->CommitToGPUVisibleHeap({ bufferCBV });

							memcpy(dst, &gpuVisiblCBV, dstByteSize);

							toState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
							transitionResource = !isInSharedHeap;
						}
						break;
						default: SEAssertF("Invalid type");
						}
					}
					break;
					default: SEAssertF("Invalid descriptor type");
					}

					if (transitionResource)
					{
						SEAssert(!isInSharedHeap, "Trying to transition a resource in a shared heap. This is unexpected");
						SEAssert(toState != D3D12_RESOURCE_STATE_COMMON, "Unexpected to state");

						resourceTransitions.emplace_back(dx12::CommandList::TransitionMetadata{
							.m_resource = bufferPlatParams->m_resolvedGPUResource,
							.m_toState = toState,
							.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES }
							});
					}
				};

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				bufferInput.GetShaderName(),
				sbt.m_rayGenShaders,
				sbtPlatParams->m_rayGenRegionBaseOffset,
				sbtPlatParams->m_rayGenRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				bufferInput.GetShaderName(),
				sbt.m_missShaders,
				sbtPlatParams->m_missRegionBaseOffset,
				sbtPlatParams->m_missRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				bufferInput.GetShaderName(),
				sbt.m_hitGroupNamesAndShaders | std::views::transform(
					[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
					{
						return hitGroupNamesAndShader.second;
					}),
				sbtPlatParams->m_hitGroupRegionBaseOffset,
				sbtPlatParams->m_hitGroupRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				bufferInput.GetShaderName(),
				sbt.m_callableShaders,
				sbtPlatParams->m_callableRegionBaseOffset,
				sbtPlatParams->m_callableRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);
		}

		// Finally, record the resource transitions:
		cmdList->TransitionResources(std::move(resourceTransitions));
	}


	void ShaderBindingTable::SetRWTextureOnLocalRoots(
		re::ShaderBindingTable const& sbt,
		re::RWTextureInput const& rwTexInput,
		dx12::GPUDescriptorHeap* gpuDescHeap,
		uint64_t currentFrameNum)
	{
		dx12::ShaderBindingTable::PlatformParams const* sbtPlatParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();

		auto SetData = [&rwTexInput, gpuDescHeap]
			(uint8_t* dst, uint8_t dstByteSize, dx12::RootSignature::RootParameter const* rootParam)
			{
				SEAssert(rootParam->m_type == RootSignature::RootParameter::Type::DescriptorTable,
					"We currently assume all textures belong to descriptor tables");

				SEAssert(rootParam->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV,
					"Trying to set a UAV on a descriptor table entry for a different type");

				D3D12_CPU_DESCRIPTOR_HANDLE const& texUAV =
					dx12::Texture::GetUAV(rwTexInput.m_texture, rwTexInput.m_textureView);

				D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleTexUAV = gpuDescHeap->CommitToGPUVisibleHeap({ texUAV });

				memcpy(dst, &gpuVisibleTexUAV, dstByteSize);
			};

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				rwTexInput.m_shaderName,
				sbt.m_rayGenShaders,
				sbtPlatParams->m_rayGenRegionBaseOffset,
				sbtPlatParams->m_rayGenRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				rwTexInput.m_shaderName,
				sbt.m_missShaders,
				sbtPlatParams->m_missRegionBaseOffset,
				sbtPlatParams->m_missRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				rwTexInput.m_shaderName,
				sbt.m_hitGroupNamesAndShaders | std::views::transform(
					[](auto const& hitGroupNamesAndShader) -> core::InvPtr<re::Shader> const&
					{
						return hitGroupNamesAndShader.second;
					}),
				sbtPlatParams->m_hitGroupRegionBaseOffset,
				sbtPlatParams->m_hitGroupRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);

			WriteSBTElement(
				sbtPlatParams->m_SBT.get(),
				SetData,
				rwTexInput.m_shaderName,
				sbt.m_callableShaders,
				sbtPlatParams->m_callableRegionBaseOffset,
				sbtPlatParams->m_callableRegionByteStride,
				sbtPlatParams->m_frameRegionByteSize,
				currentFrameNum,
				sbtPlatParams->m_numFramesInFlight);
	}


	D3D12_DISPATCH_RAYS_DESC ShaderBindingTable::BuildDispatchRaysDesc(
		re::ShaderBindingTable const& sbt, glm::uvec3 const& threadDimensions)
	{
		dx12::ShaderBindingTable::PlatformParams const* platParams =
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();

		const D3D12_GPU_VIRTUAL_ADDRESS sbtGPUVA = platParams->m_SBT->GetGPUVirtualAddress();

		// Zero out a region's start address if no shaders exist
		const bool hasHitGroupRegion = platParams->m_hitGroupRegionTotalByteSize > 0;
		const bool hasCallableRegion = platParams->m_callableRegionTotalByteSize > 0;		

		return D3D12_DISPATCH_RAYS_DESC{
			.RayGenerationShaderRecord = D3D12_GPU_VIRTUAL_ADDRESS_RANGE{
				.StartAddress = sbtGPUVA + platParams->m_rayGenRegionBaseOffset,
				.SizeInBytes = platParams->m_rayGenRegionTotalByteSize,
			},
			.MissShaderTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
				.StartAddress = sbtGPUVA + platParams->m_missRegionBaseOffset,
				.SizeInBytes = platParams->m_missRegionTotalByteSize,
				.StrideInBytes = platParams->m_missRegionByteStride,
			},
			.HitGroupTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
				.StartAddress = sbtGPUVA + platParams->m_hitGroupRegionBaseOffset * hasHitGroupRegion,
				.SizeInBytes = platParams->m_hitGroupRegionTotalByteSize,
				.StrideInBytes = platParams->m_hitGroupRegionByteStride,
			},
			.CallableShaderTable = D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE{
				.StartAddress = (sbtGPUVA + platParams->m_callableRegionBaseOffset) * hasCallableRegion,
				.SizeInBytes = platParams->m_callableRegionTotalByteSize,
				.StrideInBytes = platParams->m_callableRegionByteStride,
			},
			.Width = threadDimensions.x,
			.Height = threadDimensions.y,
			.Depth = threadDimensions.z,
		};
	}
}