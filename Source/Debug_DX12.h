// © 2022 Adam Badke. All rights reserved.
#pragma once


struct ID3D12Object;

namespace dx12
{
	inline extern bool CheckHResult(HRESULT hr, char const* msg);
	inline extern void EnableDebugLayer();
	inline extern std::wstring GetWDebugName(ID3D12Object*);
	inline extern std::string GetDebugName(ID3D12Object*);
}