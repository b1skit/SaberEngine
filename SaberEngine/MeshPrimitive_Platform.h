#pragma once

#include <memory>


namespace re
{
	class MeshPrimitive;
}


namespace platform
{
	class MeshPrimitive
	{
	public:
		// API-specific Mesh_Platform interface
		// Note: Ensure inheritance from this interface is virtual
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			static void CreatePlatformParams(re::MeshPrimitive& meshPrimitive);
		};


		static void (*Create)(re::MeshPrimitive& meshPrimitive);
		static void (*Bind)(platform::MeshPrimitive::PlatformParams const* params, bool doBind);
		static void (*Destroy)(re::MeshPrimitive& meshPrimitive);
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::MeshPrimitive::PlatformParams::~PlatformParams() {};
}
