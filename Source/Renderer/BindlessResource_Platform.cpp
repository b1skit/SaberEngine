// © 2025 Adam Badke. All rights reserved.
#include "BindlessResource_Platform.h"


namespace platform
{
	void (*AccelerationStructureResource::GetPlatformResource)(gr::AccelerationStructureResource const&, void*, size_t) = nullptr;
	void (*AccelerationStructureResource::GetDescriptor)(gr::AccelerationStructureResource const&, void*, size_t, uint8_t) = nullptr;
	void (*AccelerationStructureResource::GetResourceUseState)(gr::AccelerationStructureResource const&, void*, size_t) = nullptr;


	// ---


	void (*BufferResource::GetPlatformResource)(re::BufferResource const&, void*, size_t) = nullptr;
	void (*BufferResource::GetDescriptor)(re::BufferResource const&, void*, size_t, uint8_t) = nullptr;


	// ---


	void (*TextureResource::GetPlatformResource)(re::TextureResource const&, void*, size_t) = nullptr;
	void (*TextureResource::GetDescriptor)(re::TextureResource const&, void*, size_t, uint8_t) = nullptr;
	void (*TextureResource::GetResourceUseState)(re::TextureResource const&, void*, size_t) = nullptr;


	// ---


	void (*VertexStreamResource::GetPlatformResource)(re::VertexStreamResource const&, void*, size_t) = nullptr;
	void (*VertexStreamResource::GetDescriptor)(re::VertexStreamResource const&, void*, size_t, uint8_t) = nullptr;
}