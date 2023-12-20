// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Transform.h"
#include "RenderDataIDs.h"


namespace fr
{
	class GameplayManager;


	// EnTT wrapper for a gr::Transform, to guarantee pointer stability.
	// Automatically assigns itself a unique TransformID
	class TransformComponent
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


		// ECS_CONVERSION TODO: Move the gr::Transform to fr ????????

	public:
		struct NewIDMarker {}; // Attached when a a new TransformID is allocated

		struct RenderData
		{
			glm::mat4 g_model = glm::mat4(1.f); // Global TRS
			glm::mat4 g_transposeInvModel = glm::mat4(1.f);

			gr::TransformID m_transformID = gr::k_invalidTransformID;
		};

		static RenderData GetRenderData(gr::Transform&);

		static TransformComponent& AttachTransformComponent(fr::GameplayManager&, entt::entity, gr::Transform* parent);


	public:
		gr::Transform& GetTransform();
		gr::Transform const& GetTransform() const;

		gr::TransformID GetTransformID() const;


	public: // Transform systems:
		static void DispatchTransformUpdateThread(
			std::vector<std::future<void>>& taskFuturesOut, gr::Transform* rootNode);


	private:
		gr::Transform m_transform;
		const gr::TransformID m_transformID;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		TransformComponent(PrivateCTORTag, gr::Transform* parent);


	private: // Static TransformID functionality:
		static std::atomic<gr::TransformID> s_transformIDs;
	};


	inline gr::Transform& TransformComponent::GetTransform()
	{
		return m_transform;
	}


	inline gr::Transform const& TransformComponent::GetTransform() const
	{
		return m_transform;
	}


	inline gr::TransformID TransformComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	class UpdateTransformDataRenderCommand
	{
	public:
		UpdateTransformDataRenderCommand(fr::TransformComponent&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::TransformID m_transformID;
		const fr::TransformComponent::RenderData m_data;
	};	
}