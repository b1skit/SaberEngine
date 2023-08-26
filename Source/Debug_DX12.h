// © 2022 Adam Badke. All rights reserved.
#pragma once

struct ID3D12Object;
enum D3D12_RESOURCE_STATES;
enum D3D_FEATURE_LEVEL;


namespace dx12
{
	inline extern bool CheckHResult(HRESULT hr, char const* msg);
	inline extern void EnableDebugLayer();
	inline extern std::wstring GetWDebugName(ID3D12Object*);
	inline extern std::string GetDebugName(ID3D12Object*);
	inline extern constexpr char const* GetResourceStateAsCStr(D3D12_RESOURCE_STATES state);
	inline extern constexpr char const* GetFeatureLevelAsCStr(D3D_FEATURE_LEVEL);
}