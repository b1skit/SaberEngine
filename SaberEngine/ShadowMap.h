#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "Texture.h"
#include "TextureTarget.h"
#include "Camera.h"


namespace gr
{
	class Transform;

	class ShadowMap
	{
	public:
		ShadowMap(std::string lightName,
			uint32_t xRes,
			uint32_t yRes,
			gr::Camera::CameraConfig shadowCamConfig,
			gr::Transform* shadowCamParent = nullptr,
			glm::vec3 shadowCamPosition = glm::vec3(0.0f, 0.0f, 0.0f), // TODO: Remove default values
			bool useCubeMap = false);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		ShadowMap() = delete;

		// Get the current shadow camera
		inline std::shared_ptr<gr::Camera> ShadowCamera() { return m_shadowCam; }
		inline std::shared_ptr<gr::Camera const> ShadowCamera() const { return m_shadowCam; }

		inline float& MaxShadowBias() { return m_maxShadowBias; }
		inline float const MaxShadowBias() const { return m_maxShadowBias; }
		
		inline float& MinShadowBias() { return m_minShadowBias; }
		inline float const MinShadowBias() const { return m_minShadowBias; }

		inline glm::vec2 MinMaxShadowBias() const { return glm::vec2(m_minShadowBias, m_maxShadowBias); }
		// TODO: Store min, max shadow bias as a vec2

		inline gr::TextureTargetSet& GetTextureTargetSet() { return m_shadowTargetSet; }
		inline gr::TextureTargetSet const& GetTextureTargetSet() const { return m_shadowTargetSet; }


	private:
		// Registed in the SceneManager's currentScene
		std::shared_ptr<gr::Camera>	m_shadowCam = nullptr; 
		gr::TextureTargetSet m_shadowTargetSet;

		float m_maxShadowBias = 0.005f;	// Small offsets for shadow comparisons
		float m_minShadowBias = 0.0005f;
	};
}


