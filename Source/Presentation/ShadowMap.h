// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"

#include "Renderer/ShadowMapRenderData.h"


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
		ShadowMap(fr::Light::Type lightType);

		~ShadowMap() = default;
		ShadowMap(ShadowMap const&) = default;
		ShadowMap(ShadowMap&&) noexcept = default;
		ShadowMap& operator=(ShadowMap const&) = default;
		ShadowMap& operator=(ShadowMap&&) noexcept = default;

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


	public:
		struct TypeProperties
		{
			struct Orthographic
			{
				enum FrustumSnapMode : uint8_t
				{
					SceneBounds,
					ActiveCamera,

					FrustumSnap_Count,
				} m_frustumSnapMode;

				static constexpr std::array<char const*, FrustumSnap_Count> k_frustumSnapModeNames =
				{
					"SceneBounds",
					"ActiveCamera"
				};
				SEStaticAssert(k_frustumSnapModeNames.size() == FrustumSnap_Count, "");
			};

			union
			{
				Orthographic m_orthographic;
			};

			const ShadowType m_shadowType;
			const fr::Light::Type m_lightType;
		};
		TypeProperties const& GetTypeProperties(ShadowType) const;
		

	private:
		TypeProperties m_typeProperties;

		ShadowQuality m_shadowQuality;

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
		return m_typeProperties.m_shadowType;
	}


	inline fr::Light::Type ShadowMap::GetOwningLightType() const
	{
		return m_typeProperties.m_lightType;
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


	inline ShadowMap::TypeProperties const& ShadowMap::GetTypeProperties(ShadowType shadowType) const
	{
		SEAssert(shadowType == m_typeProperties.m_shadowType, "Trying to access type properties for the wrong type");
		return m_typeProperties;
	}
}
