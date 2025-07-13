// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RenderDataComponent.h"

#include <Renderer/RenderDataManager.h>


namespace pr
{
	std::atomic<gr::RenderDataID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent* RenderDataComponent::GetCreateRenderDataComponent(
		pr::EntityManager& em, entt::entity entity, gr::TransformID transformID)
	{
		RenderDataComponent* renderDataCmpt = em.TryGetComponent<pr::RenderDataComponent>(entity);

		SEAssert(!renderDataCmpt || renderDataCmpt->GetTransformID() == transformID,
			"RenderDataComponent already exists, but is associated with a different TransformID");

		if (!renderDataCmpt)
		{
			em.EmplaceComponent<pr::RenderDataComponent::NewRegistrationMarker>(entity);
			renderDataCmpt = em.EmplaceComponent<pr::RenderDataComponent>(entity, PrivateCTORTag{}, transformID);
		}
		
		return renderDataCmpt;
	}


	RenderDataComponent& RenderDataComponent::AttachSharedRenderDataComponent(
		pr::EntityManager& em, entt::entity entity, RenderDataComponent const& renderDataComponent)
	{
		em.EmplaceComponent<pr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<pr::RenderDataComponent>(
			entity, 
			PrivateCTORTag{},
			renderDataComponent.m_renderDataID, 
			renderDataComponent.m_transformID);
	}


	void RenderDataComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity owningEntity)
	{
		ImGui::Indent();

		pr::RenderDataComponent const& renderDataCmpt = em.GetComponent<pr::RenderDataComponent>(owningEntity);
		ImGui::Text(std::format("RenderDataID: {}, TransformID: {}",
			renderDataCmpt.GetRenderDataID(), renderDataCmpt.GetTransformID()).c_str());

		ImGui::Unindent();
	}


	void RenderDataComponent::ShowImGuiWindow(std::vector<pr::RenderDataComponent const*> const& renderDataComponents)
	{		
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
		constexpr int numCols = 2;
		if (ImGui::BeginTable("m_IDToRenderObjectMetadata", numCols, flags))
		{
			// Headers:				
			ImGui::TableSetupColumn("RenderObjectID");
			ImGui::TableSetupColumn("TransformID");
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < renderDataComponents.size(); i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// RenderDataID
				ImGui::Text(std::format("{}", renderDataComponents[i]->GetRenderDataID()).c_str());

				ImGui::TableNextColumn();

				// TransformID
				ImGui::Text(std::format("{}", renderDataComponents[i]->GetTransformID()).c_str());
			}

			ImGui::EndTable();
		}
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::TransformID transformID)
		: m_renderDataID(s_objectIDs.fetch_add(1)) // Allocate a new RenderDataID
		, m_transformID(transformID)
		, m_featureBits(0)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::RenderDataID renderObjectID, gr::TransformID transformID)
		: m_renderDataID(renderObjectID)
		, m_transformID(transformID)
		, m_featureBits(0)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, RenderDataComponent const& sharedRenderDataComponent)
		: m_renderDataID(sharedRenderDataComponent.m_renderDataID) // Shared RenderDataID
		, m_transformID(sharedRenderDataComponent.m_transformID)
		, m_featureBits(0)
	{
	}


	gr::RenderDataID RenderDataComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	gr::TransformID RenderDataComponent::GetTransformID() const
	{
		return m_transformID;
	}


	void RenderDataComponent::SetFeatureBit(gr::RenderObjectFeature feature)
	{
		SEAssert(feature != gr::RenderObjectFeature::Invalid, "Invalid feature");
		m_featureBits.fetch_or(feature);
	}


	bool RenderDataComponent::HasFeatureBit(gr::RenderObjectFeature feature) const
	{
		SEAssert(feature != gr::RenderObjectFeature::Invalid, "Invalid feature");
		return (m_featureBits.load() & feature);
	}


	gr::FeatureBitmask RenderDataComponent::GetFeatureBits() const
	{
		return m_featureBits.load();
	}


	// ---


	RegisterRenderObject::RegisterRenderObject(RenderDataComponent const& newRenderDataComponent)
		: m_renderDataID(newRenderDataComponent.GetRenderDataID())
		, m_transformID(newRenderDataComponent.GetTransformID())
		, m_featureBits(newRenderDataComponent.GetFeatureBits())
	{
	}


	void RegisterRenderObject::Execute(void* cmdData)
	{
		RegisterRenderObject* cmdPtr = reinterpret_cast<RegisterRenderObject*>(cmdData);

		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.RegisterObject(cmdPtr->m_renderDataID, cmdPtr->m_transformID);
		renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
	}


	void RegisterRenderObject::Destroy(void* cmdData)
	{
		RegisterRenderObject* cmdPtr = reinterpret_cast<RegisterRenderObject*>(cmdData);
		cmdPtr->~RegisterRenderObject();
	}


	// ---


	DestroyRenderObject::DestroyRenderObject(gr::RenderDataID objectID)
		: m_renderDataID(objectID)
	{
	}


	void DestroyRenderObject::Execute(void* cmdData)
	{
		DestroyRenderObject* cmdPtr = reinterpret_cast<DestroyRenderObject*>(cmdData);

		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.DestroyObject(cmdPtr->m_renderDataID);
	}


	void DestroyRenderObject::Destroy(void* cmdData)
	{
		DestroyRenderObject* cmdPtr = reinterpret_cast<DestroyRenderObject*>(cmdData);
		cmdPtr->~DestroyRenderObject();
	}


	// ---


	SetRenderDataFeatureBits::SetRenderDataFeatureBits(
		gr::RenderDataID renderDataID, gr::FeatureBitmask featureBits)
		: m_renderDataID(renderDataID)
		, m_featureBits(featureBits)
	{
	}


	void SetRenderDataFeatureBits::Execute(void* cmdData)
	{
		SetRenderDataFeatureBits* cmdPtr = reinterpret_cast<SetRenderDataFeatureBits*>(cmdData);

		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.SetFeatureBits(cmdPtr->m_renderDataID, cmdPtr->m_featureBits);
	}


	void SetRenderDataFeatureBits::Destroy(void* cmdData)
	{
		SetRenderDataFeatureBits* cmdPtr = reinterpret_cast<SetRenderDataFeatureBits*>(cmdData);
		cmdPtr->~SetRenderDataFeatureBits();
	}
}