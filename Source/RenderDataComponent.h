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
	struct RenderDataComponent
	{
		static RenderDataComponent& AttachRenderDataComponent(
			fr::GameplayManager&, entt::entity, uint32_t expectedNumPrimitives);


		
		
		RenderDataComponent(RenderDataComponent&&) noexcept;
		RenderDataComponent& operator=(RenderDataComponent&&) noexcept;

		size_t GetNumRenderObjectIDs() const;
		gr::RenderObjectID GetRenderObjectID(size_t index) const;
		void AddRenderObject();

	private:
		std::vector<gr::RenderObjectID> m_objectIDs;
		mutable std::shared_mutex m_objectIDsMutex;


	private:
		static std::atomic<gr::RenderObjectID> s_objectIDs;

	
	private: // No copying allowed
		RenderDataComponent() = delete;
		RenderDataComponent(RenderDataComponent const&) = delete;
		RenderDataComponent& operator=(RenderDataComponent const&) = delete;


	private: // Use AttachRenderDataComponent()
		RenderDataComponent(uint32_t expectedNumPrimitives);
	};


	// ---


	class CreateRenderObjectCommand
	{
	public:
		CreateRenderObjectCommand(gr::RenderObjectID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderObjectID m_objectID;
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
