// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/TextUtils.h"

#include <d3dx12.h>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;


namespace
{
	constexpr char const* D3D12_AUTO_BREADCRUMB_OP_ToCStr(D3D12_AUTO_BREADCRUMB_OP breadcrumbOp)
	{
		constexpr char const* k_breadcrumbOpNames[] = {
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_SETMARKER),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ENDEVENT),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DISPATCH),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_COPYTILES ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_PRESENT ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64 ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1 ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2 ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1 ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1 ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA ),
			ENUM_TO_STR(D3D12_AUTO_BREADCRUMB_OP_BARRIER)
		};

		return k_breadcrumbOpNames[breadcrumbOp];
	}


	constexpr char const* D3D12_DRED_ALLOCATION_TYPE_ToCStr(D3D12_DRED_ALLOCATION_TYPE allocationType)
	{
		constexpr char const* k_allocationTypeNames[] = {
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_FENCE),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_RESOURCE),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_PASS),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_METACOMMAND),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP),
			ENUM_TO_STR(D3D12_DRED_ALLOCATION_TYPE_INVALID)
		};
		return k_allocationTypeNames[allocationType];
	}


	void HandleDRED()
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		ComPtr<ID3D12DeviceRemovedExtendedData> dredQuery;
		SEAssert(SUCCEEDED(context->GetDevice().GetD3DDisplayDevice()->QueryInterface(IID_PPV_ARGS(&dredQuery))),
			"Failed to get DRED query interface");

		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT dredAutoBreadcrumbsOutput;
		SEAssert(SUCCEEDED(dredQuery->GetAutoBreadcrumbsOutput(&dredAutoBreadcrumbsOutput)),
			"Failed to get DRED auto breadcrumbs output");

		// Breadcrumbs:
		D3D12_AUTO_BREADCRUMB_NODE const* breadcrumbHead = dredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
		const uint32_t numBreadcrumbs = 
			breadcrumbHead ? dredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode->BreadcrumbCount : 0;

		LOG_ERROR("DRED BREADCRUMBS (%d):\n-----------------", numBreadcrumbs);
		for (uint32_t curBreadcrumb = 0; breadcrumbHead != nullptr && curBreadcrumb < numBreadcrumbs; curBreadcrumb++)
		{
			LOG_ERROR("Command list: %s", 
				breadcrumbHead->pCommandListDebugNameW ? util::FromWideString(breadcrumbHead->pCommandListDebugNameW).c_str() : "<null pCommandListDebugNameW>");
			LOG_ERROR("Command queue: %s", 
				breadcrumbHead->pCommandQueueDebugNameW ? util::FromWideString(breadcrumbHead->pCommandQueueDebugNameW).c_str() : "<null pCommandQueueDebugNameW>");
			LOG_ERROR("Breadcrumb count: %d", 
				breadcrumbHead->BreadcrumbCount);
			LOG_ERROR("Last breadcrumb value: %d", 
				breadcrumbHead->pLastBreadcrumbValue ? *breadcrumbHead->pLastBreadcrumbValue : -1);
			LOG_ERROR("Command history: %s", 
				breadcrumbHead->pCommandHistory ? D3D12_AUTO_BREADCRUMB_OP_ToCStr(*breadcrumbHead->pCommandHistory) : "<null pCommandHistory>");

			breadcrumbHead = breadcrumbHead->pNext;
		}
		
		// Page fault allocation output:
		D3D12_DRED_PAGE_FAULT_OUTPUT dredPageFaultOutput;
		SEAssert(SUCCEEDED(dredQuery->GetPageFaultAllocationOutput(&dredPageFaultOutput)),
			"Failed to get DRED page fault allocation output");

		LOG_ERROR("DRED PAGE FAULT OUTPUT:\n-----------------------");
		LOG_ERROR("Page fault virtual address: %llu\n", dredPageFaultOutput.PageFaultVA);

		LOG_ERROR("Existing allocation nodes:\n--------------------------");
		D3D12_DRED_ALLOCATION_NODE const* existingAllocationNode = dredPageFaultOutput.pHeadExistingAllocationNode;
		while (existingAllocationNode != nullptr)
		{
			LOG_ERROR("Object name: %s\nAllocation type: %s", 
				util::FromWideString(existingAllocationNode->ObjectNameW).c_str(),
				D3D12_DRED_ALLOCATION_TYPE_ToCStr(existingAllocationNode->AllocationType));

			existingAllocationNode = existingAllocationNode->pNext;
		}
		

		LOG_ERROR("Recently freed allocation nodes:\n--------------------------------");
		D3D12_DRED_ALLOCATION_NODE const* recentFreedAllocationNode = dredPageFaultOutput.pHeadRecentFreedAllocationNode;
		while (recentFreedAllocationNode != nullptr)
		{
			LOG_ERROR("Object name: %s\nAllocation type: %s",
				util::FromWideString(recentFreedAllocationNode->ObjectNameW).c_str(),
				D3D12_DRED_ALLOCATION_TYPE_ToCStr(recentFreedAllocationNode->AllocationType));

			recentFreedAllocationNode = recentFreedAllocationNode->pNext;
		}
	}
}

namespace dx12
{
	bool CheckHResult(HRESULT hr, char const* msg)
	{
		switch (hr)
		{
		case S_OK:
		{
			return true;
		}
		break;
		case S_FALSE:
		case DXGI_STATUS_OCCLUDED: // 0x087a0001
		{
			SEAssertF(
				"Checked HRESULT of a success code. Use the SUCCEEDED or FAILED macros instead of calling this function");
		}
		break;
		case DXGI_ERROR_DEVICE_REMOVED: LOG_ERROR("%s: Device removed", msg); break;
		case E_ABORT:					LOG_ERROR("%s: Operation aborted", msg); break;
		case E_ACCESSDENIED:			LOG_ERROR("%s: General access denied error", msg); break;
		case E_FAIL:					LOG_ERROR("%s: Unspecified failure", msg); break;
		case E_HANDLE:					LOG_ERROR("%s: Handle that is not valid", msg); break;
		case E_INVALIDARG:				LOG_ERROR("%s: One or more arguments are invalid", msg); break;
		case E_NOINTERFACE:				LOG_ERROR("%s: No such interface supported", msg); break;
		case E_NOTIMPL:					LOG_ERROR("%s: Not implemented", msg); break;
		case E_OUTOFMEMORY:				LOG_ERROR("%s: Failed to allocate necessary memory", msg); break;
		case E_POINTER:					LOG_ERROR("%s: Pointer that is not valid", msg); break;
		case E_UNEXPECTED:				LOG_ERROR("%s: Unexpected failure", msg); break;
		case ERROR_FILE_NOT_FOUND:		LOG_ERROR("File not found: %s", msg); break;
		default:
		{
			LOG_ERROR(msg);
		}
		}

		// DRED reporting:
		if (hr == DXGI_ERROR_DEVICE_REMOVED && 
			core::Config::Get()->GetValue<int>(core::configkeys::k_debugLevelCmdLineArg) >= 3)
		{
			HandleDRED();
		}

#if defined(_DEBUG)
		SEAssertF(msg);
#else
		throw std::exception(); // Throw an exception here; asserts are disabled in release mode
#endif

		return false;
	}


	void EnableDebugLayer()
	{
		ComPtr<ID3D12Debug> debugInterface;

		const int debugLevel = core::Config::Get()->GetValue<int>(core::configkeys::k_debugLevelCmdLineArg);

		// Enable the debug layer for debuglevel 1 and above:
		if (debugLevel >= 1)
		{
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
			dx12::CheckHResult(hr, "Failed to get debug interface");
			debugInterface->EnableDebugLayer();

			LOG("Debug level %d: Enabled D3D12 debug layer", debugLevel);
		}

		// Enable GPU-based validation for -debuglevel 2 and above:
		if (debugLevel >= 2)
		{
			ComPtr<ID3D12Debug1> debugInterface1;
			HRESULT hr = debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1));
			CheckHResult(hr, "Failed to get query interface");
			debugInterface1->SetEnableGPUBasedValidation(true);

			LOG("Debug level %d: Enabled D3D12 GPU-based validation", debugLevel);
		}

		const bool dredEnabled = core::Config::Get()->KeyExists(core::configkeys::k_enableDredCmdLineArg);
		if (dredEnabled)
		{
			ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));
			CheckHResult(hr, "Failed to get DRED interface");

			// Turn on AutoBreadcrumbs and Page Fault reporting
			dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

			LOG("D3D12 DRED enabled");
		}
	}


	std::wstring GetWDebugName(ID3D12Object* object)
	{
		constexpr uint32_t k_nameLength = 1024;
		uint32_t nameLength = k_nameLength;
		wchar_t extractedname[k_nameLength]{ '\0' };
		object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &nameLength, &extractedname);
		
		if (nameLength > 0)
		{
			return std::wstring(extractedname);
		}
		return L"<No debug name set>";
	}


	std::string GetDebugName(ID3D12Object* object)
	{
		return util::FromWideString(GetWDebugName(object));
	}


	constexpr char const* GetResourceStateAsCStr(D3D12_RESOURCE_STATES state)
	{
		switch (state)
		{
		case D3D12_RESOURCE_STATE_COMMON: return "COMMON|PRESENT";
		case D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER: return "VERTEX_AND_CONSTANT_BUFFER";
		case D3D12_RESOURCE_STATE_INDEX_BUFFER: return "INDEX_BUFFER";
		case D3D12_RESOURCE_STATE_RENDER_TARGET: return "RENDER_TARGET";
		case D3D12_RESOURCE_STATE_UNORDERED_ACCESS: return "UNORDERED_ACCESS";
		case D3D12_RESOURCE_STATE_DEPTH_WRITE: return "DEPTH_WRITE";
		case D3D12_RESOURCE_STATE_DEPTH_READ: return "DEPTH_READ";
		case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE: return "NON_PIXEL_SHADER_RESOURCE";
		case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE: return "PIXEL_SHADER_RESOURCE";
		case D3D12_RESOURCE_STATE_STREAM_OUT: return "STREAM_OUT";
		case D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT: return "INDIRECT_ARGUMENT|PREDICATION";
		case D3D12_RESOURCE_STATE_COPY_DEST: return "COPY_DEST";
		case D3D12_RESOURCE_STATE_COPY_SOURCE: return "COPY_SOURCE";
		case D3D12_RESOURCE_STATE_RESOLVE_DEST: return "RESOLVE_DEST";
		case D3D12_RESOURCE_STATE_RESOLVE_SOURCE: return "RESOLVE_SOURCE";
		case D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE: return "RAYTRACING_ACCELERATION_STRUCTURE";
		case D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE: return "SHADING_RATE_SOURCE";
		case D3D12_RESOURCE_STATE_GENERIC_READ: return "GENERIC_READ";
		case D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE: return "ALL_SHADER_RESOURCE";
		case D3D12_RESOURCE_STATE_VIDEO_DECODE_READ: return "VIDEO_DECODE_READ";
		case D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE: return "VIDEO_DECODE_WRITE";
		case D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ: return "VIDEO_PROCESS_READ";
		case D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE: return "VIDEO_PROCESS_WRITE";
		case D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ: return "VIDEO_ENCODE_READ";
		case D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE: return "VIDEO_ENCODE_WRITE";
		// Combinations:
		case D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE: return "D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE";
		default:
			return "Invalid D3D12_RESOURCE_STATES received";
		}
	}


	constexpr char const* GetFeatureLevelAsCStr(D3D_FEATURE_LEVEL featureLevel)
	{
		switch (featureLevel)
		{
		case D3D_FEATURE_LEVEL_1_0_GENERIC: return "D3D_FEATURE_LEVEL_1_0_GENERIC";
		case D3D_FEATURE_LEVEL_1_0_CORE: return "D3D_FEATURE_LEVEL_1_0_CORE";
		case D3D_FEATURE_LEVEL_9_1: return "D3D_FEATURE_LEVEL_9_1";
		case D3D_FEATURE_LEVEL_9_2: return "D3D_FEATURE_LEVEL_9_2";
		case D3D_FEATURE_LEVEL_9_3: return "D3D_FEATURE_LEVEL_9_3";
		case D3D_FEATURE_LEVEL_10_0: return "D3D_FEATURE_LEVEL_10_0";
		case D3D_FEATURE_LEVEL_10_1: return "D3D_FEATURE_LEVEL_10_1";
		case D3D_FEATURE_LEVEL_11_0: return "D3D_FEATURE_LEVEL_11_0";
		case D3D_FEATURE_LEVEL_11_1: return "D3D_FEATURE_LEVEL_11_1";
		case D3D_FEATURE_LEVEL_12_0: return "D3D_FEATURE_LEVEL_12_0";
		case D3D_FEATURE_LEVEL_12_1: return "D3D_FEATURE_LEVEL_12_1";
		case D3D_FEATURE_LEVEL_12_2: return "D3D_FEATURE_LEVEL_12_2";
		default: return "INVALID FEATURE LEVEL";
		}
		return "Invalid D3D_FEATURE_LEVEL received";
	}


	constexpr char const* D3D12ResourceBindingTierToCStr(D3D12_RESOURCE_BINDING_TIER bindingTier)
	{
		switch (bindingTier)
		{
		case D3D12_RESOURCE_BINDING_TIER_1: return "D3D12_RESOURCE_BINDING_TIER_1";
		case D3D12_RESOURCE_BINDING_TIER_2: return "D3D12_RESOURCE_BINDING_TIER_2";
		case D3D12_RESOURCE_BINDING_TIER_3: return "D3D12_RESOURCE_BINDING_TIER_3";
		default: return "Invalid D3D12_RESOURCE_BINDING_TIER received";
		}
	}


	constexpr char const* D3D12ResourceHeapTierToCStr(D3D12_RESOURCE_HEAP_TIER heapTier)
	{
		switch (heapTier)
		{
		case D3D12_RESOURCE_HEAP_TIER_1: return "D3D12_RESOURCE_HEAP_TIER_1";
		case D3D12_RESOURCE_HEAP_TIER_2: return "D3D12_RESOURCE_HEAP_TIER_2";
		default: return "Invalid D3D12_RESOURCE_HEAP_TIER received";
		}
	}
}