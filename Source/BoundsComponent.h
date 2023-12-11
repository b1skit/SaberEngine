// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Bounds.h"
#include "RenderDataComponent.h"



namespace fr
{
	class GameplayManager;


	struct IsSceneBoundsMarker {}; //Unique: Only added to 1 bounds component for the entire scene

	void CreateSceneBoundsEntity(fr::GameplayManager&);

	void AttachBoundsComponent(fr::GameplayManager&, entt::entity);


	class UpdateBoundsDataRenderCommand
	{
	public:
		UpdateBoundsDataRenderCommand(gr::RenderObjectID, gr::Bounds const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
		const gr::Bounds m_boundsData;
	};


	class DestroyBoundsDataRenderCommand
	{
	public:
		DestroyBoundsDataRenderCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
	};
}