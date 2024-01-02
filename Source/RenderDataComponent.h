// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "RenderManager.h"


namespace fr
{
	class EntityManager;
}

namespace gr
{
	// Automatically assigns itself a unique RenderDataID
	class RenderDataComponent
	{
	public:
		struct NewRegistrationMarker {}; // Attached when a a new RenderDataID is allocated

	public:
		static RenderDataComponent& AttachNewRenderDataComponent(
			fr::EntityManager&, entt::entity, gr::TransformID);

		static RenderDataComponent& AttachSharedRenderDataComponent(
			fr::EntityManager&, entt::entity, RenderDataComponent const&);

	public:
		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity);


	public:
		RenderDataComponent(RenderDataComponent&&) = default;
		RenderDataComponent& operator=(RenderDataComponent&&) = default;


	public:
		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		void SetFeature(gr::RenderObjectFeature);
		gr::FeatureBitmask GetFeatureBits() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		gr::FeatureBitmask m_featureBits;


	private:
		static std::atomic<gr::RenderDataID> s_objectIDs;


	private: // No copying allowed
		RenderDataComponent() = delete;
		RenderDataComponent(RenderDataComponent const&) = delete;
		RenderDataComponent& operator=(RenderDataComponent const&) = delete;
		

	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		
	public:
		RenderDataComponent(PrivateCTORTag, gr::TransformID); // Allocate a new RenderDataID
		RenderDataComponent(PrivateCTORTag, gr::RenderDataID, gr::TransformID); // Allocate a new RenderDataID
		RenderDataComponent(PrivateCTORTag, RenderDataComponent const&); // Shared RenderDataID
	};


	// ---


	class RegisterRenderObjectCommand
	{
	public:
		RegisterRenderObjectCommand(RenderDataComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;
		const gr::FeatureBitmask m_featureBits;
	};


	// ---


	class DestroyRenderObjectCommand
	{
	public:
		DestroyRenderObjectCommand(gr::RenderDataID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
	};


	// ---


	template<typename T>
	class UpdateRenderDataRenderCommand
	{
	public:
		UpdateRenderDataRenderCommand(gr::RenderDataID, T const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const T m_data;
	};


	template<typename T>
	UpdateRenderDataRenderCommand<T>::UpdateRenderDataRenderCommand(gr::RenderDataID objectID, T const& data)
		: m_renderDataID(objectID)
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
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.SetObjectData(cmdPtr->m_renderDataID, &cmdPtr->m_data);
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
		DestroyRenderDataRenderCommand(gr::RenderDataID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
	};


	template<typename T>
	DestroyRenderDataRenderCommand<T>::DestroyRenderDataRenderCommand(gr::RenderDataID objectID)
		: m_renderDataID(objectID)
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
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObjectData<T>(cmdPtr->m_renderDataID);
		}
	}


	template<typename T>
	void DestroyRenderDataRenderCommand<T>::Destroy(void* cmdData)
	{
		DestroyRenderDataRenderCommand<T>* cmdPtr = reinterpret_cast<DestroyRenderDataRenderCommand<T>*>(cmdData);
		cmdPtr->~DestroyRenderDataRenderCommand();
	}


	// ---


	class RenderDataFeatureBitsRenderCommand
	{
	public:
		RenderDataFeatureBitsRenderCommand(gr::RenderDataID, gr::FeatureBitmask);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::FeatureBitmask m_featureBits;
	};
}
