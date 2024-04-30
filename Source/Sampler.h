// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core\Interfaces\IPlatformParams.h"
#include "Sampler_Platform.h"
#include "Core\Interfaces\INamedObject.h"


namespace re
{
	class Sampler final : public virtual core::INamedObject
	{
	public:
		struct PlatformParams : public core::IPlatformParams
		{
			virtual ~PlatformParams() = 0;
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

		struct SamplerDesc
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

	
		// Sampler library:
	public:
		static std::shared_ptr<re::Sampler> const GetSampler(std::string const& samplerName);
		static void DestroySamplerLibrary();

	private:
		static std::unique_ptr<std::unordered_map<std::string, std::shared_ptr<re::Sampler>>> m_samplerLibrary;
		static std::recursive_mutex m_samplerLibraryMutex;


	public:
		[[nodiscard]] static std::shared_ptr<re::Sampler> Create(std::string const& name, SamplerDesc const&);

		~Sampler();

		Sampler(Sampler&& rhs) = default;
		Sampler& operator=(Sampler&&) = default;

		SamplerDesc const& GetSamplerDesc() const { return m_samplerDesc; }

		void SetPlatformParams(std::unique_ptr<Sampler::PlatformParams> params) { m_platformParams = std::move(params); }
		Sampler::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }


	private:
		Sampler(std::string const&, SamplerDesc const&);


	private:
		const SamplerDesc m_samplerDesc;
		std::unique_ptr<Sampler::PlatformParams> m_platformParams;

		
	private:
		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Sampler::PlatformParams::~PlatformParams() {};
}