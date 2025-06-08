// © 2025 Adam Badke. All rights reserved.
#include "Private/BindlessResource.h"
#include "Private/BindlessResource_Platform.h"
#include "Private/Texture.h"


namespace re
{
	void AccelerationStructureResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) const
	{
		return platform::AccelerationStructureResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void AccelerationStructureResource::GetDescriptor(
		void* descriptorOut, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const
	{
		return platform::AccelerationStructureResource::GetDescriptor(*this, descriptorOut, descriptorOutByteSize, frameOffsetIdx);
	}


	void AccelerationStructureResource::GetResourceUseState(void* dest, size_t destByteSize) const
	{
		return platform::AccelerationStructureResource::GetResourceUseState(*this, dest, destByteSize);
	}


	// ---


	void BufferResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) const
	{
		return platform::BufferResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void BufferResource::GetDescriptor(void* descriptorOut, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const
	{
		return platform::BufferResource::GetDescriptor(*this, descriptorOut, descriptorOutByteSize, frameOffsetIdx);
	}


	// ---


	void TextureResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) const
	{
		return platform::TextureResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void TextureResource::GetDescriptor(void* descriptorOut, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const
	{
		return platform::TextureResource::GetDescriptor(*this, descriptorOut, descriptorOutByteSize, frameOffsetIdx);
	}


	void TextureResource::GetResourceUseState(void* dest, size_t destByteSize) const
	{
		return platform::TextureResource::GetResourceUseState(*this, dest, destByteSize);
	}


	// ---


	void VertexStreamResource::GetPlatformResource(void* resourceOut, size_t resourceOutByteSize) const
	{
		return platform::VertexStreamResource::GetPlatformResource(*this, resourceOut, resourceOutByteSize);
	}


	void VertexStreamResource::GetDescriptor(
		void* descriptorOut, size_t descriptorOutByteSize, uint8_t frameOffsetIdx) const
	{
		return platform::VertexStreamResource::GetDescriptor(*this, descriptorOut, descriptorOutByteSize, frameOffsetIdx);
	}
}