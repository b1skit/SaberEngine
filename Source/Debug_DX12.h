// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace dx12
{
	inline extern bool CheckHResult(HRESULT hr, char const* msg);
	inline extern void EnableDebugLayer();
}