#pragma once

#include <memory>


// Pre-declarations:
namespace gr
{
	class Mesh;
}


namespace platform
{
	class Mesh
	{
	public:
		// API-specific Mesh_Platform interface
		// Note: Ensure inheritance from this interface is virtual
		struct PlatformParams
		{
			virtual ~PlatformParams() = 0;

			static std::unique_ptr<PlatformParams> CreatePlatformParams();
		};


		static void (*Create)(gr::Mesh& mesh);
		static void (*Bind)(gr::Mesh& mesh, bool doBind);
		static void (*Destroy)(gr::Mesh& mesh);
	};


	// We need to provide a destructor implementation since it's pure virutal
	inline platform::Mesh::PlatformParams::~PlatformParams() {};
}