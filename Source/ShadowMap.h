// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Camera.h"
#include "NamedObject.h"
#include "ShadowMapRenderData.h"
#include "Texture.h"
#include "TextureTarget.h"


namespace fr
{
	class Transform;
	class Light;


	class ShadowMap : public virtual en::NamedObject
	{
	public:
		enum class ShadowType : uint8_t
		{
			Orthographic, // Single 2D texture
			CubeMap,

			ShadowType_Count
		};
		static_assert(static_cast<uint8_t>(fr::ShadowMap::ShadowType::ShadowType_Count) == 
			static_cast<uint8_t>(gr::ShadowMap::ShadowType::ShadowType_Count));


	public:
		ShadowMap(
			std::string const& lightName,
			uint32_t xRes,
			uint32_t yRes,
			fr::Transform* shadowCamParent,
			fr::Light::LightType lightType,
			fr::Transform* owningTransform);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		fr::Camera* ShadowCamera();
		fr::Camera const* ShadowCamera() const;

		void UpdateShadowCameraConfig(); // Should be called any time the owning light has moved

		void SetMinMaxShadowBias(glm::vec2 const&);
		glm::vec2 const& GetMinMaxShadowBias() const;

		// DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		std::shared_ptr<re::TextureTargetSet> GetTextureTargetSet() const;

		void ShowImGuiWindow();


	private:
		const ShadowType m_shadowType;
		const fr::Light::LightType m_lightType;

		fr::Transform* m_owningTransform; // DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		std::shared_ptr<fr::Camera> m_shadowCam; // DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		std::shared_ptr<re::TextureTargetSet> m_shadowTargetSet; // DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		glm::vec2 m_minMaxShadowBias; // Small offsets for shadow comparisons


	private:
		ShadowMap() = delete;
	};


	inline fr::Camera* ShadowMap::ShadowCamera()
	{
		return m_shadowCam.get();
	}


	inline fr::Camera const* ShadowMap::ShadowCamera() const
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


