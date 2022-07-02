#pragma once

#include <memory>


// Pre-declarations:
namespace gr
{
	class Mesh;
}


namespace re::platform
{
	// API-specific Mesh_Platform interface
	// Note: Ensure inheritance from this interface is virtual
	struct MeshParams_Platform
	{
		virtual ~MeshParams_Platform() = 0;

		// Static object factory:
		static std::unique_ptr<MeshParams_Platform> Create();
	};

	// We need to provide a destructor implementation since it's pure virutal
	inline re::platform::MeshParams_Platform::~MeshParams_Platform() {};
}
