// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"
#include "ShadowMapRenderData.h"


namespace fr
{
	class Transform;
	class Light;
	class Camera;


	class ShadowMap
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

		static constexpr gr::ShadowMap::ShadowType GetRenderDataShadowMapType(fr::ShadowMap::ShadowType);


	public:
		ShadowMap(glm::uvec2 widthHeight, fr::Light::LightType lightType);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		glm::uvec2 const& GetWidthHeight() const;

		void SetMinMaxShadowBias(glm::vec2 const&);
		glm::vec2 const& GetMinMaxShadowBias() const;

		ShadowType GetShadowMapType() const;
		fr::Light::LightType GetOwningLightType() const;

		bool IsDirty() const;
		void MarkClean();

		void ShowImGuiWindow(uint64_t uniqueID);


	private:
		const ShadowType m_shadowType;
		const fr::Light::LightType m_lightType;

		glm::uvec2 m_widthHeight;

		glm::vec2 m_minMaxShadowBias; // Small offsets for shadow comparisons

		bool m_isDirty;


	private:
		ShadowMap() = delete;
	};


	inline constexpr gr::ShadowMap::ShadowType ShadowMap::GetRenderDataShadowMapType(
		fr::ShadowMap::ShadowType frShadowMapType)
	{
		switch (frShadowMapType)
		{
		case fr::ShadowMap::ShadowType::Orthographic: return gr::ShadowMap::ShadowType::Orthographic;
		case fr::ShadowMap::ShadowType::CubeMap: return gr::ShadowMap::ShadowType::CubeMap;
		default: throw std::logic_error("Invalid light type");
		}
		return gr::ShadowMap::ShadowType::ShadowType_Count;
	}


	inline glm::uvec2 const& ShadowMap::GetWidthHeight() const
	{
		return m_widthHeight;
	}


	inline glm::vec2 const& ShadowMap::GetMinMaxShadowBias() const
	{
		return m_minMaxShadowBias;
	}


	inline ShadowMap::ShadowType ShadowMap::GetShadowMapType() const
	{
		return m_shadowType;
	}


	inline fr::Light::LightType ShadowMap::GetOwningLightType() const
	{
		return m_lightType;
	}


	inline bool ShadowMap::IsDirty() const
	{
		return m_isDirty;
	}


	inline void ShadowMap::MarkClean()
	{
		m_isDirty = false;
	}
}


