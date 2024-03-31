// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"
#include "ShadowMapRenderData.h"


namespace fr
{
	class Light;


	class ShadowMap
	{
	public:
		enum class ShadowType : uint8_t
		{
			Orthographic,
			Perspective,
			CubeMap,

			ShadowType_Count
		};
		static_assert(static_cast<uint8_t>(fr::ShadowMap::ShadowType::ShadowType_Count) == 
			static_cast<uint8_t>(gr::ShadowMap::ShadowType::ShadowType_Count));

		static constexpr gr::ShadowMap::ShadowType GetGrShadowMapType(fr::ShadowMap::ShadowType);

		enum class ShadowQuality : uint8_t
		{
			PCF = 0,
			PCSS_LOW = 1,
			PCSS_HIGH = 2,

			ShadowQuality_Count
		};
		static_assert(static_cast<uint8_t>(fr::ShadowMap::ShadowQuality::ShadowQuality_Count) ==
			static_cast<uint8_t>(gr::ShadowMap::ShadowQuality::ShadowQuality_Count));

		static constexpr gr::ShadowMap::ShadowQuality GetGrShadowQuality(fr::ShadowMap::ShadowQuality);


	public:
		ShadowMap(glm::uvec2 widthHeight, fr::Light::Type lightType);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) = default;
		ShadowMap& operator=(ShadowMap const&) = default;

		glm::uvec2 const& GetWidthHeight() const;

		void SetMinMaxShadowBias(glm::vec2 const&);
		glm::vec2 const& GetMinMaxShadowBias() const;

		float GetSoftness() const;

		ShadowType GetShadowMapType() const;
		fr::Light::Type GetOwningLightType() const;

		ShadowQuality GetShadowQuality() const;

		bool IsEnabled() const;

		bool IsDirty() const;
		void MarkClean();

		void ShowImGuiWindow(uint64_t uniqueID);


	private:
		const ShadowType m_shadowType;
		const fr::Light::Type m_lightType;
		ShadowQuality m_shadowQuality;

		glm::uvec2 m_widthHeight;

		glm::vec2 m_minMaxShadowBias;
		float m_softness;

		bool m_isEnabled;

		bool m_isDirty;


	private:
		ShadowMap() = delete;
	};


	inline constexpr gr::ShadowMap::ShadowType ShadowMap::GetGrShadowMapType(
		fr::ShadowMap::ShadowType frShadowMapType)
	{
		switch (frShadowMapType)
		{
		case fr::ShadowMap::ShadowType::Orthographic: return gr::ShadowMap::ShadowType::Orthographic;
		case fr::ShadowMap::ShadowType::Perspective: return gr::ShadowMap::ShadowType::Perspective;
		case fr::ShadowMap::ShadowType::CubeMap: return gr::ShadowMap::ShadowType::CubeMap;
		default: throw std::logic_error("Invalid light type");
		}
		return gr::ShadowMap::ShadowType::ShadowType_Count;
	}


	inline constexpr gr::ShadowMap::ShadowQuality ShadowMap::GetGrShadowQuality(fr::ShadowMap::ShadowQuality quality)
	{
		switch (quality)
		{
		case fr::ShadowMap::ShadowQuality::PCF: return gr::ShadowMap::ShadowQuality::PCF;
		case fr::ShadowMap::ShadowQuality::PCSS_LOW: return gr::ShadowMap::ShadowQuality::PCSS_LOW;
		case fr::ShadowMap::ShadowQuality::PCSS_HIGH: return gr::ShadowMap::ShadowQuality::PCSS_HIGH;
		default: SEAssertF("Invalid quality");
		}
		return gr::ShadowMap::ShadowQuality::PCF;
	}


	inline glm::uvec2 const& ShadowMap::GetWidthHeight() const
	{
		return m_widthHeight;
	}


	inline glm::vec2 const& ShadowMap::GetMinMaxShadowBias() const
	{
		return m_minMaxShadowBias;
	}


	inline float ShadowMap::GetSoftness() const
	{
		return m_softness;
	}


	inline ShadowMap::ShadowType ShadowMap::GetShadowMapType() const
	{
		return m_shadowType;
	}


	inline fr::Light::Type ShadowMap::GetOwningLightType() const
	{
		return m_lightType;
	}


	inline fr::ShadowMap::ShadowQuality  fr::ShadowMap::GetShadowQuality() const
	{
		return m_shadowQuality;
	}

	inline bool ShadowMap::IsEnabled() const
	{
		return m_isEnabled;
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
