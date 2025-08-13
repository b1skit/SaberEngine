// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/RenderObjectIDs.h"


namespace gr
{
	class Camera;
}

namespace pr
{
	class EntityManager;
	class NameComponent;
	class TransformComponent;


	class CameraComponent final
	{
	public:
		struct CameraConceptMarker final {}; // Identifies cameras created via CreateCameraConcept (e.g. scene cameras)
		struct MainCameraMarker final {};
		struct NewMainCameraMarker final {};

	public:
		static void CreateCameraConcept(
			pr::EntityManager&, entt::entity sceneNode, std::string_view name, gr::Camera::Config const&);

		static pr::CameraComponent& AttachCameraComponent(
			pr::EntityManager&, entt::entity owningEntity, std::string_view name, gr::Camera::Config const&);

		static void MarkDirty(EntityManager&, entt::entity cameraEntity);

		static gr::Camera::RenderData CreateRenderData(pr::EntityManager&, entt::entity, CameraComponent const&);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity camEntity);


	public:
		pr::Camera& GetCameraForModification();
		pr::Camera const& GetCamera() const;

		gr::TransformID GetTransformID() const;


	private:
		const gr::TransformID m_transformID;

		pr::Camera m_camera;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		CameraComponent(PrivateCTORTag, gr::Camera::Config const&, pr::TransformComponent&);
	};



	inline pr::Camera& CameraComponent::GetCameraForModification()
	{
		return m_camera;
	}


	inline pr::Camera const& CameraComponent::GetCamera() const
	{
		return m_camera;
	}


	inline gr::TransformID CameraComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	class SetActiveCameraRenderCommand final : public virtual gr::RenderCommand
	{
	public:
		SetActiveCameraRenderCommand(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);

		static void Execute(void*);

	private:
		const gr::RenderDataID m_cameraRenderDataID;
		const gr::TransformID m_cameraTransformID;
	};
}