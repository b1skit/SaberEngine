// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource_VertexStream.h"
#include "BindlessResource_VertexStream_Platform.h"

#include "Core/InvPtr.h"


namespace re
{
	void IVertexStreamResourceSet::GetNullDescriptor(void* dest, size_t destByteSize) const
	{
		platform::VertexStreamResourceSet::GetNullDescriptor(*this, dest, destByteSize);
	}


	void IVertexStreamResourceSet::GetResourceUsageState(void* dest, size_t destByteSize) const
	{
		platform::VertexStreamResourceSet::GetResourceUsageState(*this, dest, destByteSize);
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


	ResourceHandle IVertexStreamResource::GetResourceHandle(VertexBufferInput const& vertexBufferInput)
	{
		SEAssert(vertexBufferInput.GetStream().IsValid() &&
			vertexBufferInput.GetBuffer() &&
			vertexBufferInput.GetBuffer()->GetBindlessResourceHandle() != k_invalidResourceHandle,
			"Vertex stream is not valid for use as a bindless resource");

		return vertexBufferInput.GetBuffer()->GetBindlessResourceHandle();
	}


	ResourceHandle IVertexStreamResource::GetResourceHandle(core::InvPtr<gr::VertexStream> const& vertexStream)
	{
		SEAssert(vertexStream.IsValid() &&
			vertexStream->GetBuffer() &&
			vertexStream->GetBuffer()->GetBindlessResourceHandle() != k_invalidResourceHandle,
			"Vertex stream is not valid for use as a bindless resource");

		return vertexStream->GetBuffer()->GetBindlessResourceHandle();
	}
}