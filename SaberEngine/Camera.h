#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "SceneObject.h"
#include "TextureTarget.h"
#include "Shader.h"


namespace SaberEngine
{
	// Contains configuration specific to a cameras rendering
	struct CameraConfig
	{
		// These default values are all overwritten during camera setup

		float m_fieldOfView		= 90.0f;			// == 0 if orthographic	
		float m_near			= 1.0f;
		float m_far				= 100.0f;
		float m_aspectRatio		= 1.0f;				// == width / height

		// Orthographic rendering properties:
		bool m_isOrthographic	= false;
		float m_orthoLeft		= -5;
		float m_orthoRight		= 5;
		float m_orthoBottom		= -5;
		float m_orthoTop		= 5;

		// Render properties:
		float m_exposure		= 1.0f;
	};


	class Camera : public SceneObject
	{
	public:
		// Default constructor
		Camera(string cameraName);

		// Config constructor
		Camera(string cameraName, CameraConfig camConfig, Transform* parent = nullptr);

		~Camera() { Destroy(); }

		void Destroy();

		// SaberObject interface:
		void Update() {} // Do nothing

		// EventListener interface:
		void HandleEvent(std::shared_ptr<EventInfo const> eventInfo) {} // Do nothing

		// Getters/Setters:
		inline float const& FieldOfView() const		{ return m_cameraConfig.m_fieldOfView; }
		inline float const& Near() const			{ return m_cameraConfig.m_near; }
		inline float const& Far() const				{ return m_cameraConfig.m_far; }

		mat4 const&	View();
		mat4 const*	CubeView(); // TODO: Recompute this if the camera has moved

		inline mat4 const&	Projection() const		{ return m_projection; }
		
		// TODO: Only compute this if something has changed
		inline mat4 const&	ViewProjection()		{ return m_viewProjection = m_projection * View(); } 

		mat4 const*	CubeViewProjection();

		std::shared_ptr<Shader>& GetRenderShader() { return m_cameraShader; }
		std::shared_ptr<Shader> const& GetRenderShader() const { return m_cameraShader; }
		
		gr::TextureTargetSet& GetTextureTargetSet() { return m_camTargetSet; }
		gr::TextureTargetSet const & GetTextureTargetSet() const { return m_camTargetSet; }

		float& Exposure() { return m_cameraConfig.m_exposure; }

		// Configure this camera for deferred rendering
		void AttachGBuffer();
		// TODO: Move this to a stage owned by a graphics system, with a target set etc
		// Cameras should just do camera things.

		void DebugPrint();
	protected:


	private:
		// Helper function: Configures the camera based on the cameraConfig. MUST be called at least once during setup
		void Initialize();

		CameraConfig m_cameraConfig;

		mat4 m_view					= mat4();
		mat4 m_projection				= mat4();
		mat4 m_viewProjection			= mat4();

		vector<mat4> m_cubeView;
		vector<mat4> m_cubeViewProjection;
		
		std::shared_ptr<Shader> m_cameraShader = nullptr;

		gr::TextureTargetSet m_camTargetSet;

		// TODO: Move initialization to ctor initialization list

		/*bool isDirty = false;*/
	};


}
