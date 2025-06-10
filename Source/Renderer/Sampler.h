// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "Sampler_Platform.h"

#include "Core/Inventory.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/IPlatformObject.h"
#include "Core/Interfaces/INamedObject.h"
#include "Core/Interfaces/IUniqueID.h"

#include "Core/Util/HashKey.h"


namespace re
{
	class Sampler final : public virtual core::INamedObject, public virtual core::IUniqueID
	{
	public:
		struct PlatObj : public core::IPlatObj
		{
			bool m_isCreated = false;
		};


	public:
		enum class FilterMode : uint8_t
		{
			MIN_MAG_MIP_POINT,
			MIN_MAG_POINT_MIP_LINEAR,
			MIN_POINT_MAG_LINEAR_MIP_POINT,
			MIN_POINT_MAG_MIP_LINEAR,
			MIN_LINEAR_MAG_MIP_POINT,
			MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			MIN_MAG_LINEAR_MIP_POINT,
			MIN_MAG_MIP_LINEAR,
			MIN_MAG_ANISOTROPIC_MIP_POINT,
			ANISOTROPIC,
			COMPARISON_MIN_MAG_MIP_POINT,
			COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
			COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
			COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
			COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
			COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
			COMPARISON_MIN_MAG_MIP_LINEAR,
			COMPARISON_MIN_MAG_ANISOTROPIC_MIP_POINT,
			COMPARISON_ANISOTROPIC,
			MINIMUM_MIN_MAG_MIP_POINT,
			MINIMUM_MIN_MAG_POINT_MIP_LINEAR,
			MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
			MINIMUM_MIN_POINT_MAG_MIP_LINEAR,
			MINIMUM_MIN_LINEAR_MAG_MIP_POINT,
			MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			MINIMUM_MIN_MAG_LINEAR_MIP_POINT,
			MINIMUM_MIN_MAG_MIP_LINEAR,
			MINIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT,
			MINIMUM_ANISOTROPIC,
			MAXIMUM_MIN_MAG_MIP_POINT,
			MAXIMUM_MIN_MAG_POINT_MIP_LINEAR,
			MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
			MAXIMUM_MIN_POINT_MAG_MIP_LINEAR,
			MAXIMUM_MIN_LINEAR_MAG_MIP_POINT,
			MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
			MAXIMUM_MIN_MAG_LINEAR_MIP_POINT,
			MAXIMUM_MIN_MAG_MIP_LINEAR,
			MAXIMUM_MIN_MAG_ANISOTROPIC_MIP_POINT,
			MAXIMUM_ANISOTROPIC
		};

		enum class EdgeMode : uint8_t
		{
			Wrap,		// Tiles at every (u,v) integer junction
			Mirror,		// Flip at every (u,v) integer junction
			Clamp,		// Coordinates outside the range [0, 1] are clamped to [0, 1]
			Border,		// Coordinates outside the range [0, 1] are set to the border color
			MirrorOnce	// Takes the absolute value of the coordinate (i.e. mirroring about 0), & then clamps to the max
		};

		enum class ComparisonFunc : uint8_t
		{
			None,	// No comparison function
			Never,
			Less,
			Equal,
			LessEqual,
			Greater,
			NotEqual,
			GreaterEqual,
			Always
		};

		enum class BorderColor
		{
			TransparentBlack,
			OpaqueBlack,
			OpaqueWhite,
			OpaqueBlack_UInt,
			OpaqueWhite_UInt
		};

		struct SamplerDesc final
		{
			FilterMode m_filterMode;
			EdgeMode m_edgeModeU;
			EdgeMode m_edgeModeV;
			EdgeMode m_edgeModeW;

			float m_mipLODBias;
			uint32_t m_maxAnisotropy;

			ComparisonFunc m_comparisonFunc;

			BorderColor m_borderColor;

			float m_minLOD;
			float m_maxLOD;

			bool operator==(SamplerDesc const& rhs) const;
		};


	public: // Convenience helpers that retrieve pre-created Samplers (only) from the Inventory
		static core::InvPtr<re::Sampler> GetSampler(util::HashKey const&);
		static core::InvPtr<re::Sampler> GetSampler(std::string_view samplerName);


	public:	
		[[nodiscard]] static core::InvPtr<re::Sampler> Create(char const*, SamplerDesc const&);
		[[nodiscard]] static core::InvPtr<re::Sampler> Create(std::string const&, SamplerDesc const&);

		Sampler(Sampler&&) noexcept = default;
		Sampler& operator=(Sampler&&) noexcept = default;

		~Sampler();

		void Destroy();

		SamplerDesc const& GetSamplerDesc() const { return m_samplerDesc; }

		void SetPlatformObject(std::unique_ptr<Sampler::PlatObj> platObj) { m_platObj = std::move(platObj); }
		Sampler::PlatObj* GetPlatformObject() const { return m_platObj.get(); }


	private:
		Sampler(char const* name, SamplerDesc const&);


	private:
		const SamplerDesc m_samplerDesc;
		std::unique_ptr<Sampler::PlatObj> m_platObj;

		
	private:
		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;
	};


	inline bool re::Sampler::SamplerDesc::operator==(SamplerDesc const& rhs) const
	{
		return m_filterMode == rhs.m_filterMode &&
			m_edgeModeU == rhs.m_edgeModeU &&
			m_edgeModeV == rhs.m_edgeModeV &&
			m_edgeModeW == rhs.m_edgeModeW &&
			m_mipLODBias == rhs.m_mipLODBias &&
			m_maxAnisotropy == rhs.m_maxAnisotropy &&
			m_comparisonFunc == rhs.m_comparisonFunc &&
			m_borderColor == rhs.m_borderColor &&
			m_minLOD == rhs.m_minLOD &&
			m_maxLOD == rhs.m_maxLOD;
	}
}