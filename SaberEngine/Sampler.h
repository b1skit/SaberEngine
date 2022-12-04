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

		platform::Sampler::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::Sampler::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }
		

		// Platform wrappers:
		void Create();
		void Bind(uint32_t textureUnit, bool doBind) const;
		void Destroy();

	private:
		SamplerParams m_samplerParams;
		std::unique_ptr<platform::Sampler::PlatformParams> m_platformParams;


	private:
		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Sampler::PlatformParams::CreatePlatformParams(gr::Sampler&);
		friend std::shared_ptr<gr::Sampler const> const GetSampler(Sampler::SamplerType type);

		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler(Sampler const&& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;
	};
}