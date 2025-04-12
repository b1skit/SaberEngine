// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace re
{
	struct AccelerationStructureResource;
	struct BufferResource;
	struct TextureResource;
	struct VertexStreamResource;
}

namespace platform
{
	struct AccelerationStructureResource
	{
	public:
		static void (*GetPlatformResource)(re::AccelerationStructureResource const&, void*, size_t);
		static void (*GetDescriptor)(re::AccelerationStructureResource const&, void*, size_t);
		static void (*GetResourceUseState)(re::AccelerationStructureResource const&, void*, size_t);
	};


	// ---


	struct BufferResource
	{
	public:
		static void (*GetPlatformResource)(re::BufferResource const&, void*, size_t);
		static void (*GetDescriptor)(re::BufferResource const&, void*, size_t);
	};


	// ---


	struct TextureResource
	{
	public:
		static void (*GetPlatformResource)(re::TextureResource const&, void*, size_t);
		static void (*GetDescriptor)(re::TextureResource const&, void*, size_t);
		static void (*GetResourceUseState)(re::TextureResource const&, void*, size_t);
	};


	// ---


	struct VertexStreamResource
	{
	public:
		static void (*GetPlatformResource)(re::VertexStreamResource const&, void*, size_t);
		static void (*GetDescriptor)(re::VertexStreamResource const&, void*, size_t);
	};
}