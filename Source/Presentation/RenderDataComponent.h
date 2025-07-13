// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderDataManager.h"
#include "Renderer/RenderObjectIDs.h"


namespace pr
{
	class EntityManager;
}

namespace pr
{
	// Automatically assigns itself a unique RenderDataID
	class RenderDataComponent
	{
	public:
		struct NewRegistrationMarker {}; // Attached when a a new RenderDataID is allocated

	public:
		static RenderDataComponent* GetCreateRenderDataComponent(
			pr::EntityManager&, entt::entity, gr::TransformID);

		static RenderDataComponent& AttachSharedRenderDataComponent(
			pr::EntityManager&, entt::entity, RenderDataComponent const&);

	public:
		static void ShowImGuiWindow(pr::EntityManager&, entt::entity owningEntity);
		static void ShowImGuiWindow(std::vector<pr::RenderDataComponent const*> const&);


	public:
		~RenderDataComponent() = default;
		RenderDataComponent(RenderDataComponent&&) noexcept = default;
		RenderDataComponent& operator=(RenderDataComponent&&) noexcept = default;


	public:
		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		void SetFeatureBit(gr::RenderObjectFeature);
		bool HasFeatureBit(gr::RenderObjectFeature) const;
		gr::FeatureBitmask GetFeatureBits() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		std::atomic<gr::FeatureBitmask> m_featureBits; // RenderDataComponents are shared


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


	class RegisterRenderObject final : public virtual gr::RenderCommand
	{
	public:
		RegisterRenderObject(RenderDataComponent const&);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;
		const gr::FeatureBitmask m_featureBits;
	};


	// ---


	class DestroyRenderObject final : public virtual gr::RenderCommand
	{
	public:
		DestroyRenderObject(gr::RenderDataID);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
	};


	// ---


	template<typename T>
	class UpdateRenderData final : public virtual gr::RenderCommand
	{
	public:
		UpdateRenderData(gr::RenderDataID, T const&);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const T m_data;
	};


	template<typename T>
	UpdateRenderData<T>::UpdateRenderData(gr::RenderDataID objectID, T const& data)
		: m_renderDataID(objectID)
		, m_data(data)
	{
	}


	template<typename T>
	void UpdateRenderData<T>::Execute(void* cmdData)
	{
		UpdateRenderData<T>* cmdPtr = reinterpret_cast<UpdateRenderData<T>*>(cmdData);
		
		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.SetObjectData(cmdPtr->m_renderDataID, &cmdPtr->m_data);
	}


	// ---


	template<typename T>
	class DestroyRenderData final : public virtual gr::RenderCommand
	{
	public:
		DestroyRenderData(gr::RenderDataID);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
	};


	template<typename T>
	DestroyRenderData<T>::DestroyRenderData(gr::RenderDataID objectID)
		: m_renderDataID(objectID)
	{
	}


	template<typename T>
	void DestroyRenderData<T>::Execute(void* cmdData)
	{
		DestroyRenderData<T>* cmdPtr = reinterpret_cast<DestroyRenderData<T>*>(cmdData);

		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.DestroyObjectData<T>(cmdPtr->m_renderDataID);
	}


	// ---


	class SetRenderDataFeatureBits final : public virtual gr::RenderCommand
	{
	public:
		SetRenderDataFeatureBits(gr::RenderDataID, gr::FeatureBitmask);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::FeatureBitmask m_featureBits;
	};
}
