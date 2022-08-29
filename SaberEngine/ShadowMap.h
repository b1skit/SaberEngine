#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "Texture.h"
#include "TextureTarget.h"
#include "Camera.h"

using gr::Camera;
using std::string;
using glm::vec3;

#define DEFAULT_SHADOWMAP_TEXPATH	"ShadowMap"		// Shadow maps don't have a filepath...
#define DEFAULT_SHADOWMAP_COLOR		vec4(1,1,1,1)	// Default to white (max far)


namespace SaberEngine
{
	class Transform;


	class ShadowMap
	{
	public:

		ShadowMap() = delete;

		ShadowMap(string lightName,
			uint32_t xRes,
			uint32_t yRes,
			gr::Camera::CameraConfig shadowCamConfig,
			Transform* shadowCamParent = nullptr,
			vec3 shadowCamPosition = vec3(0.0f, 0.0f, 0.0f), 
			bool useCubeMap = false);

		// Get the current shadow camera
		inline std::shared_ptr<gr::Camera> ShadowCamera() { return m_shadowCam; }

		inline float& MaxShadowBias() { return m_maxShadowBias; }
		inline float& MinShadowBias() { return m_minShadowBias; }

		gr::TextureTargetSet& GetTextureTargetSet() { return m_shadowTargetSet; }
		gr::TextureTargetSet const& GetTextureTargetSet() const { return m_shadowTargetSet; }

	protected:


	private:
		// Registed in the SceneManager's currentScene, & deallocated when currentScene calls ClearCameras()
		std::shared_ptr<gr::Camera>	m_shadowCam = nullptr; 
		gr::TextureTargetSet m_shadowTargetSet;

		// TODO: Move these defaults to engine/scene config, and load bias directly from the scene light???
		float m_maxShadowBias				= 0.005f;	// Small offset for when we're making shadow comparisons
		float m_minShadowBias				= 0.0005f;
	};
}


