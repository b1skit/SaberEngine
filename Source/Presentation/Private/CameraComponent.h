// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Private/Camera.h"

#include "Renderer/RenderObjectIDs.h"


namespace gr
{
	class Camera;
}

namespace fr
{
	class EntityManager;
	class NameComponent;
	class TransformComponent;


	class CameraComponent final
	{
	public:
		struct MainCameraMarker final {};
		struct NewMainCameraMarker final {};

	public:
		static void CreateCameraConcept(
			fr::EntityManager&, entt::entity sceneNode, char const* name, gr::Camera::Config const&);

		static void AttachCameraComponent(
			fr::EntityManager&, entt::entity owningEntity, char const* name, gr::Camera::Config const&);
		static void AttachCameraComponent(
			fr::EntityManager&, entt::entity owningEntity, std::string const& name, gr::Camera::Config const&);

		static void MarkDirty(EntityManager&, entt::entity cameraEntity);

		static gr::Camera::RenderData CreateRenderData(entt::entity, CameraComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity camEntity);


	public:
		fr::Camera& GetCameraForModification();
		fr::Camera const& GetCamera() const;

		gr::TransformID GetTransformID() const;


	private:
		const gr::TransformID m_transformID;

		fr::Camera m_camera;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		CameraComponent(PrivateCTORTag, gr::Camera::Config const&, fr::TransformComponent&);
	};



	inline fr::Camera& CameraComponent::GetCameraForModification()
	{
		return m_camera;
	}


	inline fr::Camera const& CameraComponent::GetCamera() const
	{
		return m_camera;
	}


	inline gr::TransformID CameraComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	class SetActiveCameraRenderCommand final
	{
	public:
		SetActiveCameraRenderCommand(gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_cameraRenderDataID;
		const gr::TransformID m_cameraTransformID;
	};
}