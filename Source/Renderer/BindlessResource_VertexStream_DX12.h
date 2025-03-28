// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "BufferView.h"


namespace dx12
{
	class IVertexStreamResource
	{
	public:
		static void GetPlatformResource(re::IBindlessResource const&, void*, size_t);
		static void GetDescriptor(re::IBindlessResourceSet const&, re::IBindlessResource const&, void* descriptorOut, size_t descriptorOutByteSize);
	};


	class VertexStreamResourceSet
	{
	public:
		static void GetNullDescriptor(re::IBindlessResourceSet const&, void*, size_t);
		static void GetResourceUsageState(re::IBindlessResourceSet const&, void*, size_t);
	};
}