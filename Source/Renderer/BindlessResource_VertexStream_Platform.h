// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "VertexStream.h"


namespace re
{
	class VertexBufferInput;
}

namespace platform
{
	class IVertexStreamResource
	{
	public:
		static std::function<ResourceHandle(void)> (*GetRegistrationCallback)(re::VertexBufferInput const&);
		static std::function<void(ResourceHandle&)>(*GetUnregistrationCallback)(gr::VertexStream::Type);
		static void (*GetPlatformResource)(re::IBindlessResource const&, void*, size_t);
		static void (*GetDescriptor)(re::IBindlessResourceSet const&, re::IBindlessResource const&, void*, size_t);
	};


	class VertexStreamResourceSet
	{
	public:
		static void (*PopulateRootSignatureDesc)(re::IBindlessResourceSet const&, void*);
	};
}