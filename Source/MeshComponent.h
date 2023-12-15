// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Mesh.h"
#include "RenderDataIDs.h"


namespace fr
{
	class GameplayManager;

	// Instantiate a gr::Mesh object as a component attached to the given entity
	void AttachMeshComponent(fr::GameplayManager&, entt::entity); // RENAME/restructure: "Create static mesh entity"

	/* Mesh entity
	*	Components:
	*		- Name
	*		- gr::Mesh
	*		- Transform
	*		- BoundsCollection
	*		- RenderData
	* 
	* Render data:
	* -> Multiple mesh primitive objects
	*	- Each with a bounds, material, etc
	*	-> RenderDataComponent assigns/contains multiple IDs (RenderData doesn't care)
	*	- 
	* 
	* ...
	* - We store data of things that change regularly (transforms, bounds, material params)
	* - We store pointers to scene data for things that don't change (vertex streams)
	*		-> Scene data handles sharing of duplicates
	* 
	*/

	static entt::entity CreateMeshEntity(fr::GameplayManager&, char const* name);


	class MeshComponent
	{
	public:
		

	//	MeshComponent(gr::Transform* parent)
	//		: m_mesh(parent)
	//	{

	//	}

	//	gr::Mesh& GetMesh()
	//	{
	//		return m_mesh;
	//	}


	//private:
	//	gr::Mesh m_mesh;
	};

	struct MeshRenderData
	{
		MeshRenderData(gr::Mesh const& mesh);

		// Meshes can have an arbitrary number of MeshComponents (each which have their own Bounds, Material, etc). To
		// keep things clean for now, we pack them into a single RenderData object
		struct MeshPrimitiveRenderData
		{
			gr::MeshPrimitive::MeshPrimitiveParams m_meshPrimitiveParams;
			gr::Material const* m_material;
			std::array<re::VertexStream const*, gr::MeshPrimitive::Slot_Count> m_vertexStreams;
			re::VertexStream const* m_indexStream;
		};

		std::vector<MeshPrimitiveRenderData> m_meshPrimitives;
	};
}