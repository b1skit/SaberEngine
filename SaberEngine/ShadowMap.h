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
			gr::Transform* shadowCamParent,
			glm::vec3 shadowCamPosition,
			bool useCubeMap);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		inline gr::Camera* ShadowCamera() { return &m_shadowCam; }
		inline gr::Camera const* ShadowCamera() const { return &m_shadowCam; }

		inline glm::vec2 MinMaxShadowBias() const { return m_minMaxShadowBias; }
		inline glm::vec2& MinMaxShadowBias() { return m_minMaxShadowBias; }

		inline re::TextureTargetSet& GetTextureTargetSet() { return m_shadowTargetSet; }
		inline re::TextureTargetSet const& GetTextureTargetSet() const { return m_shadowTargetSet; }


	private:
		gr::Camera m_shadowCam;
		re::TextureTargetSet m_shadowTargetSet;

		glm::vec2 m_minMaxShadowBias; // Small offsets for shadow comparisons

	private:
		ShadowMap() = delete;
	};
}


