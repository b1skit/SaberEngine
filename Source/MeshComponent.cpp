// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "MeshComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace fr
{
	void AttachMeshComponent(fr::GameplayManager&, entt::entity entity)
	{
		// TODO....
		// Take the required gr::Mesh ctor args as parameters here
		//		-> Name should be a component attached to ALL entities
		//	-> Construct the gr::Mesh component by emplacing it (forward the ctor args?)
		//
		// -> Write multiple overloads of this function to handle the multiple gr::Mesh ctors
	}


	// ---


	entt::entity CreateMeshEntity(fr::GameplayManager& gpm, char const* name)
	{
		entt::entity meshEntity = gpm.CreateEntity(name);

		//gpm.EmplaceComponent
	}


	// ---


	MeshRenderData::MeshRenderData(gr::Mesh const& mesh)
	{
		for (auto const& meshPrimitive : mesh.GetMeshPrimitives())
		{
			m_meshPrimitives.emplace_back(MeshPrimitiveRenderData{
				.m_meshPrimitiveParams = meshPrimitive->GetMeshParams(),
				.m_material = meshPrimitive->GetMeshMaterial(),
				.m_indexStream = meshPrimitive->GetIndexStream() });

			for (uint8_t slot = 0; slot < gr::MeshPrimitive::Slot_Count; slot++)
			{
				m_meshPrimitives.back().m_vertexStreams[slot] =
					meshPrimitive->GetVertexStream(static_cast<gr::MeshPrimitive::Slot>(slot));
			}
		}
	}
}