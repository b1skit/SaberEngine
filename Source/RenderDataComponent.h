// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderDataIDs.h"
#include "RenderManager.h"


namespace fr
{
	class GameplayManager;
}

namespace gr
{
	// Automatically assigns itself a unique RenderObjectID
	class RenderDataComponent
	{
	public:
		struct NewIDMarker {}; // Attached when a a new RenderObjectID is allocated

	public:
		static RenderDataComponent& AttachNewRenderDataComponent(
			fr::GameplayManager&, entt::entity, gr::TransformID);

		static RenderDataComponent& AttachSharedRenderDataComponent(
			fr::GameplayManager&, entt::entity, RenderDataComponent const&);


	public:
		RenderDataComponent(RenderDataComponent&&) = default;
		RenderDataComponent& operator=(RenderDataComponent&&) = default;


	public:
		gr::RenderObjectID GetRenderObjectID() const;
		gr::TransformID GetTransformID() const;


	private:
		const gr::RenderObjectID m_renderObjectID;
		const gr::TransformID m_transformID;


	private:
		static std::atomic<gr::RenderObjectID> s_objectIDs;


	private: // No copying allowed
		RenderDataComponent() = delete;
		RenderDataComponent& operator=(RenderDataComponent const&) = delete;
		

	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		
	public:
		RenderDataComponent(PrivateCTORTag, gr::TransformID); // Allocate a new RenderObjectID
		RenderDataComponent(PrivateCTORTag, gr::RenderObjectID, gr::TransformID); // Allocate a new RenderObjectID
		RenderDataComponent(PrivateCTORTag, RenderDataComponent const&); // Shared RenderObjectID
	};


	// ---


	class RegisterRenderObjectCommand
	{
	public:
		RegisterRenderObjectCommand(RenderDataComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
		const gr::TransformID m_transformID;
	};


	// ---


	class DestroyRenderObjectCommand
	{
	public:
		DestroyRenderObjectCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
	};


	// ---


	template<typename T>
	class UpdateRenderDataRenderCommand
	{
	public:
		UpdateRenderDataRenderCommand(gr::RenderObjectID, T const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
		const T m_data;
	};


	template<typename T>
	UpdateRenderDataRenderCommand<T>::UpdateRenderDataRenderCommand(gr::RenderObjectID objectID, T const& data)
		: m_objectID(objectID)
		, m_data(data)
	{
	}


	template<typename T>
	void UpdateRenderDataRenderCommand<T>::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateRenderDataRenderCommand<T>* cmdPtr = reinterpret_cast<UpdateRenderDataRenderCommand<T>*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.SetObjectData(cmdPtr->m_objectID, &cmdPtr->m_data);
		}
	}


	template<typename T>
	void UpdateRenderDataRenderCommand<T>::Destroy(void* cmdData)
	{
		UpdateRenderDataRenderCommand<T>* cmdPtr = reinterpret_cast<UpdateRenderDataRenderCommand<T>*>(cmdData);
		cmdPtr->~UpdateRenderDataRenderCommand<T>();
	}


	// ---


	template<typename T>
	class DestroyRenderDataRenderCommand
	{
	public:
		DestroyRenderDataRenderCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
	};


	template<typename T>
	DestroyRenderDataRenderCommand<T>::DestroyRenderDataRenderCommand(gr::RenderObjectID objectID)
		: m_objectID(objectID)
	{
	}


	template<typename T>
	void DestroyRenderDataRenderCommand<T>::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyRenderDataRenderCommand<T>* cmdPtr = reinterpret_cast<DestroyRenderDataRenderCommand<T>*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObjectData<T>(cmdPtr->m_objectID);
		}
	}


	template<typename T>
	void DestroyRenderDataRenderCommand<T>::Destroy(void* cmdData)
	{
		DestroyRenderDataRenderCommand<T>* cmdPtr = reinterpret_cast<DestroyRenderDataRenderCommand<T>*>(cmdData);
		cmdPtr->~DestroyRenderDataRenderCommand();
	}
}
