// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource_VertexStream_Platform.h"
#include "BufferView.h"


namespace platform
{
	std::function<ResourceHandle(void)> (*IVertexStreamResource::GetRegistrationCallback)(re::VertexBufferInput const&) = nullptr;
	std::function<void(ResourceHandle&)>(*IVertexStreamResource::GetUnregistrationCallback)(re::DataType) = nullptr;
	void (*IVertexStreamResource::GetPlatformResource)(re::IBindlessResource const&, void*, size_t) = nullptr;
	void (*IVertexStreamResource::GetDescriptor)(re::IBindlessResourceSet const&, re::IBindlessResource const&, void*, size_t) = nullptr;


	// ---


	void (*VertexStreamResourceSet::GetNullDescriptor)(re::IBindlessResourceSet const&, void*, size_t) = nullptr;
	void (*VertexStreamResourceSet::GetResourceUsageState)(re::IBindlessResourceSet const&, void*, size_t) = nullptr;
}