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
	class GameplayManager;
	class TransformComponent;


	class CameraComponent
	{
	public:
		static CameraComponent& AttachCameraComponent(
			fr::GameplayManager&, entt::entity owningEntity, char const* name, gr::Camera::Config const&);

		static gr::Camera::RenderData CreateRenderData(CameraComponent&);


		// ECS_CONVERSION: WARNING: THIS MIGHT MODIFY THE TRANSFORM... NEED TO FIGURE OUT HOW TO HAVE A CONST TRANSFORM
		static gr::Camera::CameraParams BuildCameraParams(fr::Camera&);

		static void MarkDirty(GameplayManager&, entt::entity cameraEntity);


	public:
		fr::Camera& GetCamera(); // ECS_CONVERSION: THIS SHOULD ADD A DIRTY MARKER 'GetCameraAndMarkDirty'

		fr::Camera const& GetCamera() const;

		gr::TransformID GetTransformID() const;


	private:
		fr::Camera m_camera;

		const gr::TransformID m_transformID;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		CameraComponent(PrivateCTORTag, gr::Camera::Config const&, fr::TransformComponent&);
	};



	inline fr::Camera& CameraComponent::GetCamera()
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
}