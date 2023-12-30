// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Camera.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class Camera;
}

namespace fr
{
	class EntityManager;
	class NameComponent;
	class TransformComponent;


	class CameraComponent
	{
	public:
		struct MainCameraMarker {};
		struct NewMainCameraMarker {};

	public:
		static entt::entity AttachCameraConcept(
			fr::EntityManager&, entt::entity owningEntity, char const* name, gr::Camera::Config const&);
		static entt::entity AttachCameraConcept(
			fr::EntityManager&, entt::entity owningEntity, std::string const& name, gr::Camera::Config const&);

		static void MarkDirty(EntityManager&, entt::entity cameraEntity);

		static gr::Camera::RenderData CreateRenderData(CameraComponent const&);

		static void ShowImGuiWindow(fr::CameraComponent&, fr::NameComponent const&);


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


	class SetActiveCameraRenderCommand
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