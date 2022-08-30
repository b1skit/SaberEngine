#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "SceneObject.h"
#include "TextureTarget.h"
#include "Shader.h"


namespace gr
{
	class Camera : public virtual SaberEngine::SceneObject
	{
	public:
		struct CameraConfig
		{
			float m_fieldOfView = 90.0f; // == 0 if orthographic	
			float m_near = 1.0f;
			float m_far = 100.0f;
			float m_aspectRatio = 1.0f; // == width / height

			// Orthographic properties:
			bool m_isOrthographic = false;
			float m_orthoLeft = -5;
			float m_orthoRight = 5;
			float m_orthoBottom = -5;
			float m_orthoTop = 5;

			// Image properties:
			float m_exposure = 1.0f;
		};


	public:
		Camera(std::string cameraName, CameraConfig camConfig, gr::Transform* parent);
		~Camera() { Destroy(); }

		void Destroy();

		Camera() = delete;
		Camera(Camera&&) = default;
		Camera& operator=(Camera const&) = default;

		// SaberObject interface:
		void Update() override { /*Do nothing*/ }

		// EventListener interface:
		void HandleEvent(std::shared_ptr<SaberEngine::EventInfo const> eventInfo) override { /*Do nothing*/ }

		inline float const& FieldOfView() const	{ return m_cameraConfig.m_fieldOfView; }
		inline float const& Near() const		{ return m_cameraConfig.m_near; }
		inline float const& Far() const			{ return m_cameraConfig.m_far; }

		glm::mat4 GetViewMatrix() const;
		inline glm::mat4 const&	GetProjectionMatrix() const { return m_projection; }

		inline glm::mat4 GetViewProjectionMatrix() const { return m_projection * GetViewMatrix(); }
		
		std::vector<glm::mat4> const& GetCubeViewMatrix(); // TODO: Recompute this if the camera has moved
		std::vector<glm::mat4> const& GetCubeViewProjectionMatrix();

		std::shared_ptr<gr::Shader>& GetRenderShader() { return m_cameraShader; }
		std::shared_ptr<gr::Shader> const& GetRenderShader() const { return m_cameraShader; }
		
		float& GetExposure() { return m_cameraConfig.m_exposure; }
		float const& GetExposure() const { return m_cameraConfig.m_exposure; }


	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void Initialize();

		CameraConfig m_cameraConfig;

		glm::mat4 m_view;
		glm::mat4 m_projection;

		std::vector<glm::mat4> m_cubeView;
		std::vector<glm::mat4> m_cubeViewProjection;
		
		std::shared_ptr<gr::Shader> m_cameraShader;
	};


}
