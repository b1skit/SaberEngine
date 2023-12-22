// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Camera;
}

namespace fr
{
	class GameplayManager;


	class CameraComponent
	{
	public:
		static entt::entity AttachCameraComponent(
			fr::GameplayManager&, entt::entity sceneNode, gr::Camera::Config const&);

		static gr::Camera::RenderData CreateRenderData(CameraComponent const&);


	//private: // Use the static creation factories
	//	struct PrivateCTORTag { explicit PrivateCTORTag() = default; };

	};
}