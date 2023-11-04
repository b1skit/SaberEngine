// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Camera.h"
#include "NamedObject.h"
#include "Texture.h"
#include "TextureTarget.h"


namespace gr
{
	class Transform;
	class Light;


	class ShadowMap : public virtual en::NamedObject
	{
	public:
		enum class ShadowType
		{
			Orthographic, // Single 2D texture
			CubeMap,

			Invalid
		};

	public:
		ShadowMap(
			std::string const& lightName,
			uint32_t xRes,
			uint32_t yRes,
			gr::Camera::CameraConfig shadowCamConfig,
			gr::Transform* shadowCamParent,
			glm::vec3 shadowCamPosition,
			gr::Light* owningLight);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		gr::Camera* ShadowCamera();
		gr::Camera const* ShadowCamera() const;

		void UpdateShadowCameraConfig(); // Should be called any time the owning light has moved

		void SetMinMaxShadowBias(glm::vec2 const&);
		glm::vec2 const& GetMinMaxShadowBias() const;

		std::shared_ptr<re::TextureTargetSet> GetTextureTargetSet() const;

		void ShowImGuiWindow();


	private:
		const ShadowType m_shadowType;
		gr::Light* m_owningLight;
		std::shared_ptr<gr::Camera> m_shadowCam;
		std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet;

		glm::vec2 m_minMaxShadowBias; // Small offsets for shadow comparisons


	private:
		ShadowMap() = delete;
	};


	inline gr::Camera* ShadowMap::ShadowCamera()
	{
		return m_shadowCam.get();
	}


	inline gr::Camera const* ShadowMap::ShadowCamera() const
	{
		return m_shadowCam.get();
	}


	inline glm::vec2 const& ShadowMap::GetMinMaxShadowBias() const
	{
		return m_minMaxShadowBias;
	}


	inline std::shared_ptr<re::TextureTargetSet> ShadowMap::GetTextureTargetSet() const
	{
		return m_shadowTargetSet;
	}
}


