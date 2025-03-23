// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource_VertexStream.h"
#include "BindlessResource_VertexStream_Platform.h"

#include "Core/InvPtr.h"


namespace re
{
	void IVertexStreamResourceSet::PopulateRootSignatureDesc(void* dest) const
	{
		platform::VertexStreamResourceSet::PopulateRootSignatureDesc(*this, dest);
	}


	// ---


	void IVertexStreamResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize)
	{
		return platform::IVertexStreamResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void IVertexStreamResource::GetDescriptor(
		IBindlessResourceSet* resourceSet, void* descriptorOut, size_t descriptorOutByteSize)
	{
		return platform::IVertexStreamResource::GetDescriptor(*resourceSet, *this, descriptorOut, descriptorOutByteSize);
	}


	std::function<ResourceHandle(void)> IVertexStreamResource::GetRegistrationCallback(
		core::InvPtr<gr::VertexStream> const& vertexStream)
	{
		// Need to handle this at the platform layer, as the dx12::Context currently holds the BindlessResourceManager
		return platform::IVertexStreamResource::GetRegistrationCallback(re::VertexBufferInput(vertexStream));
	}


	std::function<void(ResourceHandle&)> IVertexStreamResource::GetUnregistrationCallback(
		gr::VertexStream::Type streamType)
	{
		return platform::IVertexStreamResource::GetUnregistrationCallback(streamType);
	}
}