#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Sampler_Platform.h"
#include "NamedObject.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Sampler : public virtual en::NamedObject
	{
	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			bool m_isCreated = false;
		};

	public:
		static const std::vector<std::string> SamplerTypeLibraryNames;

		enum class SamplerType
		{
			// EdgeMode, MinFilter, MaxFilter
			WrapLinearLinear,
			ClampLinearLinear,
			ClampNearestNearest,
			ClampLinearMipMapLinearLinear,
			WrapLinearMipMapLinearLinear,
			SamplerType_Count
		};
		static std::shared_ptr<gr::Sampler> const GetSampler(SamplerType type);

	private:
		static std::unique_ptr<std::unordered_map<SamplerType, std::shared_ptr<gr::Sampler>>> m_samplerLibrary;


	public:
		enum class Mode
		{
			Wrap,
			Mirrored,
			Clamp,

			Invalid,
			Mode_Count = Invalid
		};

		enum class MinFilter
		{
			Nearest,
			NearestMipMapLinear,
			Linear,
			LinearMipMapLinear,

			Invalid,
			MinFilter_Count = Invalid
		};

		enum class MaxFilter
		{
			Nearest,
			Linear,

			Invalid,
			MaxFilter_Count = Invalid
		};

		struct SamplerParams
		{
			Mode m_texSamplerMode = Mode::Wrap;
			MinFilter m_texMinMode = MinFilter::LinearMipMapLinear;
			MaxFilter m_texMaxMode = MaxFilter::Linear;
		};


	public:
		explicit Sampler(std::string const& name, SamplerParams params);
		~Sampler() { Destroy(); }

		SamplerParams const& GetSamplerParams() const { return m_samplerParams; }

		Sampler::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		Sampler::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }


	private:
		void Destroy();

	private:
		SamplerParams m_samplerParams;
		std::unique_ptr<Sampler::PlatformParams> m_platformParams;


	private:
		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Sampler::CreatePlatformParams(gr::Sampler&);
		friend std::shared_ptr<gr::Sampler const> const GetSampler(Sampler::SamplerType type);

		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler(Sampler const&& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Sampler::PlatformParams::~PlatformParams() {};
}