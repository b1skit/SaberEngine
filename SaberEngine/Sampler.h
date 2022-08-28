#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Sampler_Platform.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Sampler
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
		static std::shared_ptr<gr::Sampler const> const GetSampler(SamplerType type);

	private:
		static std::unique_ptr<std::unordered_map<SamplerType, std::shared_ptr<gr::Sampler>>> m_samplerLibrary;


	public:
		enum class TextureSamplerMode
		{
			Wrap,
			Mirrored,
			Clamp,

			Invalid,
			TextureSamplerMode_Count = Invalid
		};

		enum class TextureMinFilter
		{
			Nearest,
			NearestMipMapLinear,
			Linear,
			LinearMipMapLinear,

			Invalid,
			TextureMinificationMode_Count = Invalid
		};

		enum class TextureMaxFilter
		{
			Nearest,
			Linear,

			Invalid,
			TextureMaxificationMode_Count = Invalid
		};

		struct SamplerParams
		{
			TextureSamplerMode m_texSamplerMode = TextureSamplerMode::Wrap;
			TextureMinFilter m_texMinMode = TextureMinFilter::LinearMipMapLinear;
			TextureMaxFilter m_texMaxMode = TextureMaxFilter::Linear;
		};


	public:
		Sampler(std::string const& name, SamplerParams params);
		~Sampler() { Destroy(); }

		SamplerParams const& GetSamplerParams() const { return m_samplerParams; }

		platform::Sampler::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); }
		platform::Sampler::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

		std::string const& GetName() { return m_name; }
		

		// Platform wrappers:
		void Create(uint32_t textureUnit);
		void Bind(uint32_t textureUnit, bool doBind) const;
		void Destroy();



	public:
		Sampler() = delete;
		Sampler(Sampler const& rhs) = delete;
		Sampler(Sampler const&& rhs) = delete;
		Sampler& operator=(Sampler const& rhs) = delete;

	private:
		std::string m_name;
		SamplerParams m_samplerParams;
		std::unique_ptr<platform::Sampler::PlatformParams> m_platformParams;


		


		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Sampler::PlatformParams::CreatePlatformParams(gr::Sampler&);
		friend std::shared_ptr<gr::Sampler const> const GetSampler(Sampler::SamplerType type);
	};
}